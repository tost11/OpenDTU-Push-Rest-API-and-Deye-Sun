#include "HoymilesWInverter.h"
#include <cstring>
#include <ctime>
#include "dtuConst.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "AppGetHistPower.pb.h"
#include "RealtimeDataNew.pb.h"
#include "GetConfig.pb.h"
#include "CommandPB.pb.h"
#include "APPInfomationData.pb.h"

#undef TAG
static const char* TAG = "HoymilesW";

#define DTU_TIME_OFFSET 28800

HoymilesWInverter::HoymilesWInverter(uint64_t serial):
_client(nullptr),
_connState(ConnState::Offline),
_serverIP(""),
_serverPort(10081),
_lastSeen(0),
_pendingResponse(NONE),
_updateReceived(false),
_updateFailed(false),
_statisticsInitialized(false),
_appInfoReceived(false),
_appInfoFailed(false),
    _dtuSerial(0),
    _changeCheckedClearOnDisconnect(false){

    _serial = serial;

    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             ((uint32_t)((serial >> 32) & 0xFFFFFFFF)),
             ((uint32_t)(serial & 0xFFFFFFFF)));
    _serialString = serial_buff;

    _alarmLogParser.reset(new DefaultAlarmLog("HoymilesW"));
    _devInfoParser.reset(new HoymilesWDevInfo());
    _powerCommandParser.reset(new PowerCommandParser());
    _statisticsParser.reset(new HoymilesWStatisticsParser());
    _systemConfigParaParser.reset(new SystemConfigParaParser());

    _devInfoParser->setMaxPowerDevider(10);

    _dataUpdateTimer.set(31 * 1000);
    _dataUpdateTimer.zero();
    _dataStatisticTimer.set(10 * 60 * 1000);
    _dataStatisticTimer.zero();

    _reconnectTimer.set(HOYMILES_W_RECONNECT_DELAY_MS);
    _reconnectTimer.zero();

    _responseTimer.set(15000);
    _responseTimer.zero();

    _inverterData.currentTimestamp = 0;

    _rxBuffer.reserve(1024);
    initializeCRC();
}

HoymilesWInverter::~HoymilesWInverter() {
    if (_client) {
        _client->close(true);
        delete _client;
        _client = nullptr;
    }
}

void HoymilesWInverter::startConnection(){
    if (!_client){
        _client = new AsyncClient();
        _client->onConnect([&](void*, AsyncClient*){ this->onConnect(); });
        _client->onDisconnect([&](void*, AsyncClient*){ this->onDisconnect(); });
        _client->onError([&](void*, AsyncClient*, int8_t e){ this->onError(e); });
        _client->onData([&](void*, AsyncClient*, void* d, size_t l){ this->onDataReceived(d, l); });
        ESP_LOGI(TAG, "DTU interface setup for '%s':'%d", _serverIP.c_str(), _serverPort);
    }
    if(getEnablePolling()){
        connect();
    }
}

bool HoymilesWInverter::isConnected(){
    return _client != nullptr && _connState == ConnState::Connected;
}

void HoymilesWInverter::connect(){
    if(time(nullptr) < 1700000000){
        ESP_LOGW(TAG, "Time not synced or set, not connecting to DTU");
        return;
    }
    _dtuSerial = 0;
    if (_client && !_client->connected()){
        _connState = ConnState::Connecting;
        ESP_LOGI(TAG, "Attempting to connect to DTU '%s':'%d'", _serverIP.c_str(), _serverPort);
        if (!_client->connect(_serverIP.c_str(), _serverPort)){
            ESP_LOGI(TAG, "Connection attempt failed");
            _connState = ConnState::Error;
        }
    }
}

void HoymilesWInverter::disconnect(){
    if (_client){
        _client->stop();
    }
    if(_pendingResponse == RESTARTDEVICE || _pendingResponse == RESTARTINVERTER || _pendingResponse == POWER){
        getPowerCommand()->setLastPowerCommandSuccess(CMD_NOK);
    }
    _connState = ConnState::Offline;
    _pendingResponse = NONE;
    _dtuSerial = 0;
    _lastSeen = 0;
    _statisticsInitialized = false;
    _updateReceived = false;
    _appInfoFailed = false;
    _rxBuffer.clear();
    _reconnectTimer.reset();
}

void HoymilesWInverter::onConnect(){
    _client->setKeepAlive(10 * 1000, 5);
    _connState = ConnState::Connected;
    ESP_LOGI(TAG, "Connected to DTU");
}

void HoymilesWInverter::onDisconnect(){
    ESP_LOGI(TAG, "Disconnected from DTU");
    if(_pendingResponse == RESTARTDEVICE || _pendingResponse == RESTARTINVERTER || _pendingResponse == POWER){
        getPowerCommand()->setLastPowerCommandSuccess(CMD_NOK);
    }
    _connState = ConnState::Offline;
    _pendingResponse = NONE;
    _dtuSerial = 0;
    _lastSeen = 0;
    _statisticsInitialized = false;
    _updateReceived = false;
    _appInfoFailed = false;
    _rxBuffer.clear();
    _reconnectTimer.reset();
}

void HoymilesWInverter::onError(int8_t error){
    if(_connState == ConnState::Connected){
        if(error == ERR_RST){
            ConnectionStatistics.Disconnects++;
        }else if(error == ERR_ABRT){
            ConnectionStatistics.ConnectionTimeouts++;
        }
    }
    String errorStr = _client->errorToString(error);
    ESP_LOGI(TAG, "DTU connection error: %s (%d)", errorStr.c_str(), error);
    _connState = ConnState::Error;
    _inverterData.dtuRssi = 0;
}

void HoymilesWInverter::onDataReceived(void* data, size_t len){
    if(len == 0) return;

    uint8_t* bytes = static_cast<uint8_t*>(data);
    _rxBuffer.insert(_rxBuffer.end(), bytes, bytes + len);

    if(_rxBuffer.size() > 2048){
        ESP_LOGW(TAG, "RX buffer overflow, resetting to resync");
        _rxBuffer.clear();
        return;
    }

    processRxFrames();
}

void HoymilesWInverter::processRxFrames(){
    while(_rxBuffer.size() >= 10){
        if(_rxBuffer[0] == 0x00){
            _rxBuffer.erase(_rxBuffer.begin());
            continue;
        }

        if(_rxBuffer[0] != 0x48 || _rxBuffer[1] != 0x4D){
            ESP_LOGW(TAG, "RX resync: bad header 0x%02x", _rxBuffer[0]);
            _rxBuffer.erase(_rxBuffer.begin());
            continue;
        }

        uint16_t expectedLength = (_rxBuffer[8] << 8) | _rxBuffer[9];
        if(expectedLength < 10){
            ESP_LOGW(TAG, "RX invalid length %u, resync", expectedLength);
            _rxBuffer.erase(_rxBuffer.begin());
            continue;
        }

        if(_rxBuffer.size() < expectedLength){
            break;
        }

        uint16_t payloadLen = expectedLength - 10;
        CRC16 crc;
        crc.restart();
        for(uint16_t i = 0; i < payloadLen; i++){
            crc.add(_rxBuffer[10 + i]);
        }
        uint16_t crcTarget = (_rxBuffer[6] << 8) | _rxBuffer[7];
        if(crc.calc() != crcTarget){
            ESP_LOGW(TAG, "RX CRC mismatch got 0x%04x exp 0x%04x, resync", crc.calc(), crcTarget);
            _rxBuffer.erase(_rxBuffer.begin());
            continue;
        }

        pb_istream_t istream = pb_istream_from_buffer(&_rxBuffer[10], payloadLen);
        switch(_pendingResponse){
            case REALDATA:
                readRespRealDataNew(istream);
                if(isConnected()) writeReqGetConfig();
                break;
            case HISTPOWER:
                readRespAppGetHistPower(istream);
                break;
            case GETCONFIG:
                readRespGetConfig(istream);
                if(isConnected() && !_appInfoReceived && !_appInfoFailed) writeReqAppInfoData();
                break;
            case APPINFO:
                readRespAppInfoData(istream);
                break;
            case COMMAND:
                readRespCommand(istream);
                if(isConnected()) writeReqGetConfig();
                break;
            case RESTARTDEVICE:
                readRespCommandRestartDevice(istream);
                break;
            case RESTARTINVERTER:
                readRespCommandRestartInverter(istream);
                break;
            case POWER:
                readRespCommandPower(istream);
                break;
            default:
                ESP_LOGW(TAG, "RX frame with no pending response");
                break;
        }

        _rxBuffer.erase(_rxBuffer.begin(), _rxBuffer.begin() + expectedLength);
    }
}

void HoymilesWInverter::update(){
    getEventLog()->checkErrorsForTimeout();

    if(_IpOrHostnameIsMac){
        checkForMacResolution();
    }

    if(_resolvedIpByMacAdress != nullptr && *_resolvedIpByMacAdress != std::string(_serverIP.c_str())){
        ESP_LOGI(TAG, "Set new ip by mac to DTU interface");
        _serverIP = String(_resolvedIpByMacAdress->c_str());
        if(isConnected()) disconnect();
    }

    if(getEnablePolling()){
        if(!isConnected() && _reconnectTimer.occured()){
            _reconnectTimer.reset();
            connect();
        }
    }else{
        if(isConnected()){
            disconnect();
            return;
        }
        return;
    }

    if(!isConnected()){
        if(!_changeCheckedClearOnDisconnect){
            _changeCheckedClearOnDisconnect = true;
            if(_zeroValuesIfUnreachable){
                _statisticsParser->beginAppendFragment();
                _statisticsParser->clearBuffer();
                _statisticsParser->endAppendFragment();
            }
        }
        _dataUpdateTimer.zero();
        _dataStatisticTimer.zero();
        _responseTimer.zero();
        return;
    }

    _changeCheckedClearOnDisconnect = false;

    // Timeout-based reachability check
    if(_lastSeen && (millis() - _lastSeen > 60000)){
        ESP_LOGI(TAG, "No data for 60s, disconnecting to trigger reconnect");
        disconnect();
        return;
    }

    // Response timeout
    if(_pendingResponse != NONE && _responseTimer.occured()){
        ESP_LOGW(TAG, "Response timeout for pending request");
        if(_pendingResponse == RESTARTDEVICE || _pendingResponse == RESTARTINVERTER || _pendingResponse == POWER){
            getPowerCommand()->setLastPowerCommandSuccess(CMD_NOK);
        }
        if(_pendingResponse == APPINFO){
            _appInfoFailed = true;
        }
        _pendingResponse = NONE;
        _responseTimer.zero();
        _updateFailed = true;
        _statisticsParser->incrementRxFailureCount();
    }

    // Poll data when no pending response
    if(_pendingResponse == NONE){
        if(!_statisticsInitialized || _dataStatisticTimer.occured()){
            if(_dataUpdateTimer.occured()){
                if(requestStatisticUpdate()){
                    _dataStatisticTimer.reset();
                    _dataUpdateTimer.reset();
                }
            }
        }else{
            if(_dataUpdateTimer.occured()){
                if(requestDataUpdate()){
                    _dataUpdateTimer.reset();
                }
            }
        }
    }

    if(lastRequestFailed()){
        _statisticsParser->incrementRxFailureCount();
    }

    auto data = newDataAvailable();
    if(data != nullptr){
        _statisticsParser->resetRxFailureCount();
        _lastSeen = millis();
        printDataAsTextToSerial();
        if(isSerialValid(_serial)){
            swapBuffers(data.get());
        }else{
            ESP_LOGI(TAG, "Skipped setting inverter data - serial mismatch (dtu read %llu vs configured %llu)", getReadSerial(), _serial);
            _alarmLogParser->addAlarm(6,60,"received inverter data not used because serials not matching (correct serial and ip?)");
            _devInfoParser->setLastUpdate(millis());
        }
    }
}

void HoymilesWInverter::swapBuffers(const InverterData *data){
    _statisticsParser->beginAppendFragment();
    _statisticsParser->clearBuffer();
    _statisticsParser->appendFragment(0, (const uint8_t*)data, sizeof(InverterData));
    _statisticsParser->setLastUpdate(millis());
    _statisticsParser->resetRxFailureCount();
    _statisticsParser->endAppendFragment();

    _systemConfigParaParser->setLimitPercent(data->powerLimit);

    _devInfoParser->clearBuffer();
    _devInfoParser->setLastUpdate(millis());
}

uint64_t HoymilesWInverter::serial() const{
    return _serial;
}

String HoymilesWInverter::typeName() const{
    return _devInfoParser->getHwModelName();
}

bool HoymilesWInverter::isProducing(){
    auto stats = getStatistics();
    float totalAc = 0;
    for(auto& c : stats->getChannelsByType(TYPE_AC)){
        if(stats->hasChannelFieldValue(TYPE_AC, c, FLD_PAC)){
            totalAc += stats->getChannelFieldValue(TYPE_AC, c, FLD_PAC);
        }
    }
    return _enablePolling && totalAc > 0;
}

bool HoymilesWInverter::isReachable(){
    return isConnected() && (!_lastSeen || (millis() - _lastSeen < 60000));
}

bool HoymilesWInverter::sendActivePowerControlRequest(float limit, PowerLimitControlType type){
    uint16_t realLimit;
    if(type == RelativPersistent){
        realLimit = (uint16_t)(limit + 0.5);
    }else{
        uint16_t maxPower = _devInfoParser->getMaxPower();
        if(maxPower == 0){
            _alarmLogParser->addAlarm(6,10*60,"command not send (max Power) because no known limit for this inverter is available (serial unknown)");
            getSystemConfigParaParser()->setLastLimitRequestSuccess(CMD_NOK);
            return false;
        }
        realLimit = (uint16_t)(limit / (float)maxPower * 100);
        if(limit > 0 && realLimit == 0){
            realLimit = 1; // a positive watt value must not become "no limit" (0%)
        }
    }
    if(realLimit > 100){
        realLimit = 100;
    }
    if(!isSerialValid(_serial)){
        _alarmLogParser->addAlarm(6,10*60,"command not send (max Power) because serials not matching (correct serial and ip?)");
        return false;
    }
    setPowerLimit(realLimit);
    return true;
}

bool HoymilesWInverter::resendPowerControlRequest(){
    return false;
}

bool HoymilesWInverter::sendRestartControlRequest(){
    if(!isSerialValid(_serial)){
        _alarmLogParser->addAlarm(6,10*60,"command not send (restart) because serials not matching (correct serial and ip?)");
        return false;
    }
    requestRestartInverter();
    return true;
}

bool HoymilesWInverter::sendPowerControlRequest(bool turnOn){
    if(!isSerialValid(_serial)){
        _alarmLogParser->addAlarm(6,10*60,"command not send (power on/off) because serials not matching (correct serial and ip?)");
        return false;
    }
    requestPowerControl(turnOn);
    return true;
}

bool HoymilesWInverter::sendDtuRestartRequest(){
    requestRestartDevice();
    return true;
}

inverter_type HoymilesWInverter::getInverterType() const{
    return inverter_type::Inverter_HoymilesW;
}

void HoymilesWInverter::setEnableCommands(const bool enabled){
    BaseInverter::setEnableCommands(enabled);
}

void HoymilesWInverter::hostOrPortUpdated(){
    String ip = _oringalIpOrHostname;
    if(_resolvedIpByMacAdress != nullptr){
        ip = _resolvedIpByMacAdress->c_str();
    }
    _serverIP = ip;
    _serverPort = _port;
    if(isConnected()) disconnect();
}

bool HoymilesWInverter::supportsPowerDistributionLogic(){
    return false;
}

uint64_t HoymilesWInverter::getInternalPollTime(){
    uint64_t time = _pollTime;
    if(time < 5) time = 5;
    return time * 1000;
}

void HoymilesWInverter::onPollTimeChanged(){
    _dataUpdateTimer.setTimeout(getInternalPollTime());
}

void HoymilesWInverter::resetStats(){
    ConnectionStatistics = {};
}

String HoymilesWInverter::LogTag(){
    return TAG;
}

bool HoymilesWInverter::requestDataUpdate(){
    if(_client && _client->connected() && _pendingResponse == NONE){
        writeReqRealDataNew();
        return true;
    }
    return false;
}

bool HoymilesWInverter::requestStatisticUpdate(){
    if(_client && _client->connected() && _pendingResponse == NONE){
        writeReqAppGetHistPower();
        return true;
    }
    return false;
}

void HoymilesWInverter::setPowerLimit(int limit){
    _inverterData.powerLimitSet = limit;
    if(_client && _client->connected()){
        ESP_LOGI(TAG, "Setting power limit: %d%%", limit);
        writeReqCommand((uint8_t)limit);
    }else{
        ESP_LOGI(TAG, "Cannot set power limit - client not connected");
    }
}

void HoymilesWInverter::requestRestartDevice(){
    if(_client && _client->connected() && _pendingResponse == NONE){
        ESP_LOGI(TAG, "Requesting DTU restart");
        getPowerCommand()->setLastPowerCommandSuccess(CMD_PENDING);
        writeCommandRestartDevice();
    }else{
        ESP_LOGI(TAG, "Cannot restart DTU - client not connected or command in flight");
        getPowerCommand()->setLastPowerCommandSuccess(CMD_NOK);
    }
}

void HoymilesWInverter::requestRestartInverter(){
    if(_client && _client->connected() && _pendingResponse == NONE){
        ESP_LOGI(TAG, "Requesting inverter restart");
        getPowerCommand()->setLastPowerCommandSuccess(CMD_PENDING);
        writeCommandRestartInverter();
    }else{
        ESP_LOGI(TAG, "Cannot restart inverter - client not connected or command in flight");
        getPowerCommand()->setLastPowerCommandSuccess(CMD_NOK);
    }
}

void HoymilesWInverter::requestPowerControl(bool turnOn){
    if(_client && _client->connected() && _pendingResponse == NONE){
        ESP_LOGI(TAG, "Requesting inverter %s", turnOn ? "on" : "off");
        getPowerCommand()->setLastPowerCommandSuccess(CMD_PENDING);
        writeCommandPower(turnOn);
    }else{
        ESP_LOGI(TAG, "Cannot set inverter power on/off - client not connected or command in flight");
        getPowerCommand()->setLastPowerCommandSuccess(CMD_NOK);
    }
}

std::unique_ptr<InverterData> HoymilesWInverter::newDataAvailable(){
    std::lock_guard<std::mutex> lock(_inverterDataMutex);
    if(!_updateReceived) return nullptr;
    auto ret = std::make_unique<InverterData>(_inverterData);
    _updateReceived = false;
    return std::move(ret);
}

bool HoymilesWInverter::lastRequestFailed(){
    bool failed = _updateFailed;
    _updateFailed = false;
    return failed;
}

bool HoymilesWInverter::isSerialValid(const uint64_t serial) const{
    return _dtuSerial == serial;
}

uint64_t HoymilesWInverter::getReadSerial() const{
    return _dtuSerial;
}

bool HoymilesWInverter::statisticsReceived(){
    return _statisticsInitialized;
}

void HoymilesWInverter::printDataAsTextToSerial(){
    std::lock_guard<std::mutex> lock(_inverterDataMutex);
    ESP_LOGD(TAG, "power limit (set): %d%% (%d%%) --- ", _inverterData.powerLimit, _inverterData.powerLimitSet);
    ESP_LOGD(TAG, "inverter temp: %d C", _inverterData.inverterTemp);
    ESP_LOGD(TAG, "grid | cur: %6.2f A | vol: %6.2f V | pwr: %6.2f W | day: %8.3f kWh | tot: %8.3f kWh |",
             calcValue(_inverterData.grid.current,100),
             calcValue(_inverterData.grid.voltage),
             calcValue(_inverterData.grid.power),
             calcValue(_inverterData.grid.dailyEnergy,1000),
             calcValue(_inverterData.grid.totalEnergy,1000));
    for(int i=0; i<4; i++){
        ESP_LOGD(TAG, "pv%d | cur: %6.2f A | vol: %6.2f V | pwr: %6.2f W | day: %8.3f kWh | tot: %8.3f kWh |", i,
                 calcValue(_inverterData.pv[i].current,100),
                 calcValue(_inverterData.pv[i].voltage),
                 calcValue(_inverterData.pv[i].power),
                 calcValue(_inverterData.pv[i].dailyEnergy,1000),
                 calcValue(_inverterData.pv[i].totalEnergy,1000));
    }
}

float HoymilesWInverter::calcValue(int32_t value, int32_t divider){
    return static_cast<float>(value) / divider;
}

void HoymilesWInverter::initializeCRC(){
    _crc.setInitial(CRC16_MODBUS_INITIAL);
    _crc.setPolynome(CRC16_MODBUS_POLYNOME);
    _crc.setReverseIn(CRC16_MODBUS_REV_IN);
    _crc.setReverseOut(CRC16_MODBUS_REV_OUT);
    _crc.setXorOut(CRC16_MODBUS_XOR_OUT);
    _crc.restart();
}

// ---- PROTOBUF TX/RX ----

static String getTimeStringByTimestamp(unsigned long timestamp){
    char s[20];
    memset(s, '\0', 20);
    snprintf(s, 20, "%lu", timestamp);
    return String(s);
}

void HoymilesWInverter::writeReqRealDataNew(){
    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    RealDataNewResDTO realdatanewresdto = RealDataNewResDTO_init_default;
    realdatanewresdto.offset = DTU_TIME_OFFSET;
    realdatanewresdto.time = int32_t(time(nullptr));
    if(!pb_encode(&stream, RealDataNewResDTO_fields, &realdatanewresdto)){
        ESP_LOGI(TAG, "writeReqRealDataNew - failed to encode");
        return;
    }

    for(unsigned int i = 0; i < stream.bytes_written; i++){
        _crc.add(buffer[i]);
    }

    uint8_t header[10];
    header[0] = 0x48; header[1] = 0x4d; header[2] = 0xa3; header[3] = 0x11;
    header[4] = 0x00; header[5] = 0x01;
    header[6] = (_crc.calc() >> 8) & 0xFF; header[7] = _crc.calc() & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; header[9] = (stream.bytes_written + 10) & 0xFF;
    _crc.restart();

    uint8_t message[10 + stream.bytes_written];
    memcpy(message, header, 10);
    memcpy(message + 10, buffer, stream.bytes_written);

    _pendingResponse = REALDATA;
    _responseTimer.reset();
    _client->write((const char*)message, 10 + stream.bytes_written);
    ConnectionStatistics.SendRequests++;
}

void HoymilesWInverter::readRespRealDataNew(pb_istream_t istream){
    ConnectionStatistics.SuccessfulRequests++;
    std::lock_guard<std::mutex> lock(_inverterDataMutex);
    _pendingResponse = NONE;
    RealDataNewReqDTO realdatanewreqdto = RealDataNewReqDTO_init_default;

    PvMO pvData[4];
    for(int i = 0; i<4; i++) pvData[i] = PvMO_init_zero;

    pb_decode(&istream, &RealDataNewReqDTO_msg, &realdatanewreqdto);
    ESP_LOGD(TAG, "RealDataNew - got remote (%d): %s", realdatanewreqdto.timestamp, getTimeStringByTimestamp(realdatanewreqdto.timestamp).c_str());

    if(realdatanewreqdto.timestamp != 0){
        _inverterData.respTimestamp = uint32_t(realdatanewreqdto.timestamp);

        for(int i=0; i<4; i++) pvData[i] = realdatanewreqdto.pv_data[i];

        SGSMO gridData = SGSMO_init_zero;
        gridData = realdatanewreqdto.sgs_data[0];
        _dtuSerial = static_cast<uint64_t>(gridData.serial_number);

        _devInfoParser->setLastUpdate(millis());

        _inverterData.grid.current = static_cast<uint16_t>(gridData.current);
        _inverterData.grid.voltage = static_cast<uint16_t>(gridData.voltage);
        _inverterData.grid.power = static_cast<uint16_t>(gridData.active_power);
        _inverterData.inverterTemp = static_cast<int16_t>(gridData.temperature);
        _inverterData.gridFreq = static_cast<uint16_t>(gridData.frequency);
        _inverterData.reactivePower = static_cast<uint16_t>(gridData.reactive_power);
        _inverterData.powerFactor = static_cast<uint16_t>(gridData.power_factor);

        for(int i=0; i<4; i++){
            _inverterData.pv[i].current = static_cast<uint16_t>(pvData[i].current);
            _inverterData.pv[i].voltage = static_cast<uint16_t>(pvData[i].voltage);
            _inverterData.pv[i].power = static_cast<uint16_t>(pvData[i].power);
            _inverterData.pv[i].dailyEnergy = static_cast<uint32_t>(pvData[i].energy_daily);
            if(pvData[i].energy_total != 0)
                _inverterData.pv[i].totalEnergy = static_cast<uint32_t>(pvData[i].energy_total);
        }

        // Check timestamp update
        if((_inverterData.lastRespTimestamp != _inverterData.respTimestamp) && (_inverterData.respTimestamp != 0)){
            if(abs((int(_inverterData.respTimestamp) - int(_inverterData.currentTimestamp))) > 3){
                _inverterData.currentTimestamp = _inverterData.respTimestamp;
                ESP_LOGI(TAG, "Synced local time with DTU time");
            }
        }
        _inverterData.lastRespTimestamp = _inverterData.respTimestamp;
    }else{
        ESP_LOGI(TAG, "readRespRealDataNew - got timestamp == 0");
    }
}

void HoymilesWInverter::writeReqAppGetHistPower(){
    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    AppGetHistPowerResDTO appgethistpowerres = AppGetHistPowerResDTO_init_default;
    appgethistpowerres.offset = DTU_TIME_OFFSET;
    appgethistpowerres.requested_time = int32_t(time(nullptr));
    if(!pb_encode(&stream, AppGetHistPowerResDTO_fields, &appgethistpowerres)){
        ESP_LOGI(TAG, "writeReqAppGetHistPower - failed to encode");
        return;
    }

    for(unsigned int i = 0; i < stream.bytes_written; i++) _crc.add(buffer[i]);

    uint8_t header[10];
    header[0] = 0x48; header[1] = 0x4d; header[2] = 0xa3; header[3] = 0x15;
    header[4] = 0x00; header[5] = 0x01;
    header[6] = (_crc.calc() >> 8) & 0xFF; header[7] = _crc.calc() & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; header[9] = (stream.bytes_written + 10) & 0xFF;
    _crc.restart();

    uint8_t message[10 + stream.bytes_written];
    memcpy(message, header, 10);
    memcpy(message + 10, buffer, stream.bytes_written);

    ESP_LOGI(TAG, "Sending AppGetHistPower request to DTU");
    _pendingResponse = HISTPOWER;
    _responseTimer.reset();
    _client->write((const char*)message, 10 + stream.bytes_written);
    ConnectionStatistics.SendRequests++;
}

void HoymilesWInverter::readRespAppGetHistPower(pb_istream_t istream){
    ConnectionStatistics.SuccessfulRequests++;
    _pendingResponse = NONE;
    AppGetHistPowerReqDTO appgethistpowerreqdto = AppGetHistPowerReqDTO_init_default;
    pb_decode(&istream, &AppGetHistPowerReqDTO_msg, &appgethistpowerreqdto);

    std::lock_guard<std::mutex> lock(_inverterDataMutex);
    _inverterData.grid.dailyEnergy = appgethistpowerreqdto.daily_energy;
    _inverterData.grid.totalEnergy = appgethistpowerreqdto.total_energy;
    _statisticsInitialized = true;
}

void HoymilesWInverter::writeReqGetConfig(){
    if(!_client || !_client->connected()) return;
    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    GetConfigResDTO getconfigresdto = GetConfigResDTO_init_default;
    getconfigresdto.offset = DTU_TIME_OFFSET;
    getconfigresdto.time = int32_t(time(nullptr));
    if(!pb_encode(&stream, GetConfigResDTO_fields, &getconfigresdto)){
        ESP_LOGI(TAG, "writeReqGetConfig - failed to encode");
        return;
    }

    for(unsigned int i = 0; i < stream.bytes_written; i++) _crc.add(buffer[i]);

    uint8_t header[10];
    header[0] = 0x48; header[1] = 0x4d; header[2] = 0xa3; header[3] = 0x09;
    header[4] = 0x00; header[5] = 0x01;
    header[6] = (_crc.calc() >> 8) & 0xFF; header[7] = _crc.calc() & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; header[9] = (stream.bytes_written + 10) & 0xFF;
    _crc.restart();

    uint8_t message[10 + stream.bytes_written];
    memcpy(message, header, 10);
    memcpy(message + 10, buffer, stream.bytes_written);

    _pendingResponse = GETCONFIG;
    _responseTimer.reset();
    _client->write((const char*)message, 10 + stream.bytes_written);
    ConnectionStatistics.SendRequests++;
}

void HoymilesWInverter::readRespGetConfig(pb_istream_t istream){
    ConnectionStatistics.SuccessfulRequests++;
    _pendingResponse = NONE;
    GetConfigReqDTO getconfigreqdto = GetConfigReqDTO_init_default;
    pb_decode(&istream, &GetConfigReqDTO_msg, &getconfigreqdto);

    ESP_LOGD(TAG, "GetConfig - got remote (%d): %s", getconfigreqdto.request_time, getTimeStringByTimestamp(getconfigreqdto.request_time).c_str());

    std::lock_guard<std::mutex> lock(_inverterDataMutex);
    if(getconfigreqdto.request_time != 0){
        _inverterData.respTimestamp = uint32_t(getconfigreqdto.request_time);
    }

    int powerLimit = int(calcValue(getconfigreqdto.limit_power_mypower));
    _inverterData.powerLimit = ((powerLimit != 0) ? powerLimit : _inverterData.powerLimit);
    _inverterData.dtuRssi = getconfigreqdto.wifi_rssi;
    if(_inverterData.powerLimit > 100) _inverterData.powerLimit = 100;

    _updateReceived = true;
}

void HoymilesWInverter::writeReqAppInfoData(){
    if(!_client || !_client->connected()) return;

    time_t now = time(nullptr);
    struct tm tmInfo;
    gmtime_r(&now, &tmInfo);
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
             tmInfo.tm_year + 1900, tmInfo.tm_mon + 1, tmInfo.tm_mday,
             tmInfo.tm_hour, tmInfo.tm_min, tmInfo.tm_sec);

    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    APPInfoDataResDTO appinforesdto = APPInfoDataResDTO_init_default;
    appinforesdto.time_ymd_hms.size = 19;
    memcpy(appinforesdto.time_ymd_hms.bytes, timeStr, 19);
    appinforesdto.offset = DTU_TIME_OFFSET;
    appinforesdto.time = uint32_t(now);
    if(!pb_encode(&stream, APPInfoDataResDTO_fields, &appinforesdto)){
        ESP_LOGI(TAG, "writeReqAppInfoData - failed to encode");
        return;
    }

    for(unsigned int i = 0; i < stream.bytes_written; i++) _crc.add(buffer[i]);

    uint8_t header[10];
    header[0] = 0x48; header[1] = 0x4d; header[2] = 0xa3; header[3] = 0x01;
    header[4] = 0x00; header[5] = 0x01;
    header[6] = (_crc.calc() >> 8) & 0xFF; header[7] = (_crc.calc()) & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; header[9] = (stream.bytes_written + 10) & 0xFF;
    _crc.restart();

    uint8_t message[10 + stream.bytes_written];
    memcpy(message, header, 10);
    memcpy(message + 10, buffer, stream.bytes_written);

    ESP_LOGI(TAG, "Sending AppInfoData request to DTU");
    _pendingResponse = APPINFO;
    _responseTimer.reset();
    _client->write((const char*)message, 10 + stream.bytes_written);
    ConnectionStatistics.SendRequests++;
}

void HoymilesWInverter::readRespAppInfoData(pb_istream_t istream){
    ConnectionStatistics.SuccessfulRequests++;
    _pendingResponse = NONE;
    _appInfoReceived = true;
    APPInfoDataReqDTO appinforeqdto = APPInfoDataReqDTO_init_default;
    pb_decode(&istream, &APPInfoDataReqDTO_msg, &appinforeqdto);

    int32_t invVersion = (appinforeqdto.pv_info_count > 0) ? appinforeqdto.pv_info[0].pv_sw_version : 0;
    int32_t dtuVersion = appinforeqdto.dtu_info.dtu_sw_version;

    if(invVersion != 0){
        _devInfoParser->setFwBuildVersion(uint16_t(invVersion));
    }

    if(dtuVersion != 0){
        // bitfield: major = v/4096, minor = (v/256)%16, patch = v%256
        uint32_t major = uint32_t(dtuVersion / 4096);
        uint32_t minor = uint32_t((dtuVersion / 256) % 16);
        uint32_t patch = uint32_t(dtuVersion % 256);
        uint32_t packed = major * 10000 + minor * 100 + patch;
        if(packed <= 0xFFFF){
            _devInfoParser->setDtuFwVersion(uint16_t(packed));
        }
    }

    ESP_LOGI(TAG, "AppInfoData - dtu sw raw: %d, inverter sw raw: %d", dtuVersion, invVersion);
}

bool HoymilesWInverter::writeReqCommand(uint8_t setPercent){
    if(!_client || !_client->connected()) return false;

    uint16_t limitLevel = setPercent * 10;
    if(limitLevel > 1000) limitLevel = 1000;

    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    CommandResDTO commandresdto = CommandResDTO_init_default;
    commandresdto.time = int32_t(time(nullptr));
    commandresdto.action = CMD_ACTION_LIMIT_POWER;
    commandresdto.package_nub = 1;
    commandresdto.tid = int32_t(time(nullptr));

    const int bufferSize = 61;
    char dataArray[bufferSize];
    String dataString = "A:" + String(limitLevel) + ",B:0,C:0\r";
    dataString.toCharArray(dataArray, bufferSize);
    strcpy(commandresdto.data, dataArray);

    if(!pb_encode(&stream, CommandResDTO_fields, &commandresdto)){
        ESP_LOGI(TAG, "writeReqCommand - failed to encode");
        return false;
    }

    for(unsigned int i = 0; i < stream.bytes_written; i++) _crc.add(buffer[i]);

    uint8_t header[10];
    header[0] = 0x48; header[1] = 0x4d; header[2] = 0xa3; header[3] = 0x05;
    header[4] = 0x00; header[5] = 0x01;
    header[6] = (_crc.calc() >> 8) & 0xFF; header[7] = _crc.calc() & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; header[9] = (stream.bytes_written + 10) & 0xFF;
    _crc.restart();

    uint8_t message[10 + stream.bytes_written];
    memcpy(message, header, 10);
    memcpy(message + 10, buffer, stream.bytes_written);

    ESP_LOGD(TAG, "Sending power limit command to DTU");
    _pendingResponse = COMMAND;
    _responseTimer.reset();
    _client->write((const char*)message, 10 + stream.bytes_written);
    return true;
}

bool HoymilesWInverter::readRespCommand(pb_istream_t istream){
    _pendingResponse = NONE;
    return true;
}

bool HoymilesWInverter::writeCommandRestartDevice(){
    if(!_client || !_client->connected()){
        ESP_LOGI(TAG, "writeCommandRestartDevice - not connected");
        return false;
    }

    // Implemented per hoymiles-wifi reference (CMD_CLOUD_COMMAND_RES_DTO, action CMD_ACTION_DTU_REBOOT).
    // Never successfully tested on a real device - no reboot observed on our DTU.
    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    CommandResDTO commandresdto = CommandResDTO_init_default;
    commandresdto.action = CMD_ACTION_DTU_REBOOT;
    commandresdto.package_nub = 1;
    commandresdto.tid = int32_t(time(nullptr));

    if(!pb_encode(&stream, CommandResDTO_fields, &commandresdto)){
        ESP_LOGI(TAG, "writeCommandRestartDevice - failed to encode");
        return false;
    }

    for(unsigned int i = 0; i < stream.bytes_written; i++) _crc.add(buffer[i]);

    uint8_t header[10];
    header[0] = 0x48; header[1] = 0x4d; header[2] = 0x23; header[3] = 0x05;
    header[4] = 0x00; header[5] = 0x01;
    header[6] = (_crc.calc() >> 8) & 0xFF; header[7] = _crc.calc() & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; header[9] = (stream.bytes_written + 10) & 0xFF;
    _crc.restart();

    uint8_t message[10 + stream.bytes_written];
    memcpy(message, header, 10);
    memcpy(message + 10, buffer, stream.bytes_written);

    ESP_LOGI(TAG, "Sending DTU restart command");
    _pendingResponse = RESTARTDEVICE;
    _responseTimer.reset();
    _client->write((const char*)message, 10 + stream.bytes_written);
    ConnectionStatistics.SendRequests++;
    return true;
}

bool HoymilesWInverter::readRespCommandRestartDevice(pb_istream_t istream){
    ConnectionStatistics.SuccessfulRequests++;
    _pendingResponse = NONE;
    CommandReqDTO commandreqdto = CommandReqDTO_init_default;
    pb_decode(&istream, &CommandReqDTO_msg, &commandreqdto);
    ESP_LOGI(TAG, "DTU restart response - action: %i, err: %i", commandreqdto.action, commandreqdto.err_code);
    getPowerCommand()->setLastPowerCommandSuccess(commandreqdto.err_code == 0 ? CMD_OK : CMD_NOK);
    return true;
}

bool HoymilesWInverter::encodeMiToSn(pb_ostream_t *stream, const pb_field_t *field, void * const *arg){
    const int64_t serial = *(const int64_t*)*arg;
    if(!pb_encode_tag_for_field(stream, field)){
        return false;
    }
    return pb_encode_varint(stream, (uint64_t)serial);
}

bool HoymilesWInverter::writeCommandRestartInverter(){
    if(!_client || !_client->connected()){
        ESP_LOGI(TAG, "writeCommandRestartInverter - not connected");
        return false;
    }

    // Implemented per hoymiles-wifi reference (CMD_CLOUD_COMMAND_RES_DTO, action CMD_ACTION_INV_REBOOT,
    // dev_kind DEV_DTU, mi_to_sn). Never successfully tested on a real device.
    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    CommandResDTO commandresdto = CommandResDTO_init_default;
    commandresdto.action = CMD_ACTION_INV_REBOOT;
    commandresdto.dev_kind = DEV_DTU;
    commandresdto.package_nub = 1;
    commandresdto.tid = int32_t(time(nullptr));
    int64_t miSn = (int64_t)_serial;
    commandresdto.mi_to_sn.arg = &miSn;
    commandresdto.mi_to_sn.funcs.encode = HoymilesWInverter::encodeMiToSn;

    if(!pb_encode(&stream, CommandResDTO_fields, &commandresdto)){
        ESP_LOGI(TAG, "writeCommandRestartInverter - failed to encode");
        return false;
    }

    for(unsigned int i = 0; i < stream.bytes_written; i++) _crc.add(buffer[i]);

    uint8_t header[10];
    header[0] = 0x48; header[1] = 0x4d; header[2] = 0x23; header[3] = 0x05;
    header[4] = 0x00; header[5] = 0x01;
    header[6] = (_crc.calc() >> 8) & 0xFF; header[7] = _crc.calc() & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; header[9] = (stream.bytes_written + 10) & 0xFF;
    _crc.restart();

    uint8_t message[10 + stream.bytes_written];
    memcpy(message, header, 10);
    memcpy(message + 10, buffer, stream.bytes_written);

    ESP_LOGI(TAG, "Sending inverter restart command");
    _pendingResponse = RESTARTINVERTER;
    _responseTimer.reset();
    _client->write((const char*)message, 10 + stream.bytes_written);
    ConnectionStatistics.SendRequests++;
    return true;
}

bool HoymilesWInverter::readRespCommandRestartInverter(pb_istream_t istream){
    ConnectionStatistics.SuccessfulRequests++;
    _pendingResponse = NONE;
    CommandReqDTO commandreqdto = CommandReqDTO_init_default;
    pb_decode(&istream, &CommandReqDTO_msg, &commandreqdto);
    ESP_LOGI(TAG, "Inverter restart response - action: %i, err: %i", commandreqdto.action, commandreqdto.err_code);
    getPowerCommand()->setLastPowerCommandSuccess(commandreqdto.err_code == 0 ? CMD_OK : CMD_NOK);
    return true;
}

bool HoymilesWInverter::writeCommandPower(bool turnOn){
    if(!_client || !_client->connected()){
        ESP_LOGI(TAG, "writeCommandPower - not connected");
        return false;
    }

    // CMD_COMMAND_RES_DTO variant - tested and working on a real device.
    uint8_t buffer[200];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    CommandResDTO commandresdto = CommandResDTO_init_default;
    commandresdto.time = int32_t(time(nullptr));
    commandresdto.action = turnOn ? CMD_ACTION_MI_START : CMD_ACTION_MI_SHUTDOWN;
    commandresdto.package_nub = 1;
    commandresdto.package_now = 0;
    commandresdto.tid = int32_t(time(nullptr));
    int64_t miSn = (int64_t)_serial;
    commandresdto.mi_to_sn.arg = &miSn;
    commandresdto.mi_to_sn.funcs.encode = HoymilesWInverter::encodeMiToSn;

    if(!pb_encode(&stream, CommandResDTO_fields, &commandresdto)){
        ESP_LOGI(TAG, "writeCommandPower - failed to encode");
        return false;
    }

    for(unsigned int i = 0; i < stream.bytes_written; i++) _crc.add(buffer[i]);

    uint8_t header[10];
    header[0] = 0x48; header[1] = 0x4d; header[2] = 0xa3; header[3] = 0x05;
    header[4] = 0x00; header[5] = 0x01;
    header[6] = (_crc.calc() >> 8) & 0xFF; header[7] = _crc.calc() & 0xFF;
    header[8] = ((stream.bytes_written + 10) >> 8) & 0xFF; header[9] = (stream.bytes_written + 10) & 0xFF;
    _crc.restart();

    uint8_t message[10 + stream.bytes_written];
    memcpy(message, header, 10);
    memcpy(message + 10, buffer, stream.bytes_written);

    ESP_LOGI(TAG, "Sending inverter %s command", turnOn ? "on" : "off");
    _pendingResponse = POWER;
    _responseTimer.reset();
    _client->write((const char*)message, 10 + stream.bytes_written);
    ConnectionStatistics.SendRequests++;
    return true;
}

bool HoymilesWInverter::readRespCommandPower(pb_istream_t istream){
    ConnectionStatistics.SuccessfulRequests++;
    _pendingResponse = NONE;
    CommandReqDTO commandreqdto = CommandReqDTO_init_default;
    pb_decode(&istream, &CommandReqDTO_msg, &commandreqdto);
    ESP_LOGI(TAG, "Power on/off response - action: %i, err: %i", commandreqdto.action, commandreqdto.err_code);
    getPowerCommand()->setLastPowerCommandSuccess(commandreqdto.err_code == 0 ? CMD_OK : CMD_NOK);
    return true;
}
