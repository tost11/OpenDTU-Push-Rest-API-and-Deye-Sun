//
// Created by lukas on 31.05.25.
//

#include <mutex>
#include "CustomModbusDeyeInverter.h"
#include "DeyeUtils.h"

#undef TAG
static const char* TAG = "DeyeSun(CM)";

CustomModbusDeyeInverter::CustomModbusDeyeInverter(uint64_t serial):
DeyeInverter(serial){
    _reconnectTimeout.set(15000);
    _reconnectTimeout.zero();
    _requestDataTimeout.set(15000);
    _requestDataTimeout.zero();
    _statusPrintTimeout.set(5000);
    _statusPrintTimeout.zero();
    _failedReadCounterReset = 0;
    _pollDataTimout.set(_pollTime * 1000);
    _pollDataTimout.zero();

    //modbus frame
    int start_register = 40;
    int end_register = 116;
    std::string pos_ini = DeyeUtils::lengthToHexString(start_register,4).c_str();
    std::string pos_fin = DeyeUtils::lengthToHexString(end_register - start_register + 1,4).c_str();
    std::string businessfield = "0103" + pos_ini + pos_fin;
    std::string crc = DeyeUtils::hex_to_bytes(DeyeUtils::modbusCRC16FromASCII(businessfield));

    _requestDataCommand = createReqeustDataCommand(DeyeUtils::hex_to_bytes(businessfield) + crc);

    _client.onData([this](void * arg, AsyncClient * client,void *data, size_t len){this->onDataReceived(data,len);});
    _redBytes.store(0, std::memory_order_relaxed);

    _wasConnecting = false;
}

CustomModbusDeyeInverter::~CustomModbusDeyeInverter() {
    _client.onData(nullptr);
    _client.stop();
}

deye_inverter_type CustomModbusDeyeInverter::getDeyeInverterType() const {
    return Deye_Sun_Custom_Modbus;
}

void inline swapTwoBytes(char * buf,size_t pos){
    char cache[2];
    cache[0] = buf[pos];
    cache[1] = buf[pos + 1];
    buf[pos] = buf[pos + 2];
    buf[pos + 1] = buf[pos + 3];
    buf[pos + 2] = cache[0];
    buf[pos + 3] = cache[1];
}

void CustomModbusDeyeInverter::update() {

    getEventLog()->checkErrorsForTimeout();

    if(_statusPrintTimeout.occured()){
        ESP_LOGD(TAG, "Deye Custom Modbus -> Socket status: %s\n",_client.stateToString());
        _statusPrintTimeout.reset();
    }

    if(_IpOrHostnameIsMac){
        if(checkForMacResolution() && _resolvedIpByMacAdress != nullptr){
            //new ip found for mac
            _client.stop();
            _reconnectTimeout.zero();
        }
    }

    // Check and fetch firmware version periodically
    //checkAndFetchFirmwareVersion();

    // Check restart command result
    checkRestartCommandResult();

    //TODO think about better handling for this
    if(_currentWritCommand == nullptr){
        checkForNewWriteCommands();
    }

    if(_client.connected()){

        if(!getEnablePolling()){
            ESP_LOGI(TAG,"Deye Custom Modbus -> stop polling data");
            _client.stop();
        }

        //handle data fetching
        if(_redBytes.load(std::memory_order_acquire) > 0) {
            std::lock_guard<std::mutex> lock(_readDataLock);
            size_t redBytes = _redBytes.load(std::memory_order_relaxed);
            if (redBytes < 27) {
                ESP_LOGD(TAG,"not enough data");
                //not enoth data
                _writeTimeout.reset();
                _readTimeout.reset();
            } else if (redBytes == 29) {
                //TODO handle
                ESP_LOGD(TAG,"error response");
                //error
                _writeTimeout.reset();
                _readTimeout.reset();
            } else if (redBytes < 29 + 4) {
                ESP_LOGD(TAG,"not enoth data for valid frame");
                //not enoth data
                _writeTimeout.reset();
                _readTimeout.reset();
            } else if(_readBuffer[0] != 0xA5) {
                ESP_LOGD(TAG,"start bytes wrong");
                _writeTimeout.reset();
                _readTimeout.reset();
            } else if(_readBuffer[redBytes -1] != 0x15) {
                ESP_LOGD(TAG,"end bytes wrong");
                _writeTimeout.reset();
                _readTimeout.reset();
            } else {
                ESP_LOGD(TAG,"Received bytes are: %d", redBytes);
                if (_readTimeout.has_value()) {
                    _failedReadCounterReset = 0;
                    handleReadResponse();
                } else if (_writeTimeout.has_value()) {
                    _failedReadCounterReset = 0;
                    handleWriteResponse();
                } else {
                    ESP_LOGD(TAG,"received data but no where requested...");
                }
            }
            _redBytes.store(0, std::memory_order_release);
        }
    }

    if(_client.state() != 4) {//not connected
        if(_currentWritCommand != nullptr || _limitToSet != nullptr || _powerTargetStatus){

            if((_currentWritCommand != nullptr && _currentWritCommand->writeRegister == "0028") || _limitToSet != nullptr){
                _powerCommandParser->setLastPowerCommandSuccess(LastCommandSuccess::CMD_NOK);
            }

            _currentWritCommand = nullptr;
            _limitToSet = nullptr;
            _powerTargetStatus = nullptr;

            ESP_LOGI(TAG,"connection lost, so currently queued write commands are canceled");
        }
    }

    //polling is disabled (night whatever) wait for existing socket connection and command if null not active skip check
    if(!_client.connected() && !getEnablePolling()){
        return;
    }

    if(!_client.connected() && _reconnectTimeout.occured()){
        _reconnectTimeout.reset();
        const char * address = _resolvedIpByMacAdress == nullptr ? _oringalIpOrHostname.c_str() : _resolvedIpByMacAdress->c_str();
        _client.stop();
        _client.connect(address, _port);
        ESP_LOGI(TAG,"reconnect %s %d\n",address,_port);
        ConnectionStatistics.Connects ++;
        _wasConnecting = true;
    }

    if(_client.state() == 4){//establised
        if(_wasConnecting){
            _client.setKeepAlive(10 * 1000, 5);
            _wasConnecting = false;
            ConnectionStatistics.SuccessfulConnects++;
            _writeTimeout.reset();
            _readTimeout.reset();
            _failedReadCounterReset = 0;
        }

        if(_readTimeout.has_value() && _readTimeout->occured()){
            ESP_LOGW(TAG,"read timout hit while waiting for data");
            _readTimeout.reset();
        }

        if(_writeTimeout.has_value() && _writeTimeout->occured()){
            ESP_LOGW(TAG,"write timout hit while waiting for data");
            _writeTimeout.reset();
            _systemConfigParaParser->setLastLimitRequestSuccess(LastCommandSuccess::CMD_NOK);
        }

        if(_requestDataTimeout.occured() && !_writeTimeout.has_value() && !_readTimeout.has_value()){
            _readTimeout.emplace(COMMEND_TIMEOUT * 1000);

            ESP_LOGD(TAG,"end new read data request");
            _requestDataTimeout.reset();
            _client.write(_requestDataCommand.c_str(),_requestDataCommand.length());
            ConnectionStatistics.SendReadDataRequests++;
            _failedReadCounterReset++;
        }

        if(_failedReadCounterReset > 20){
            ESP_LOGI(TAG,"Deye Custom Modbus -> closing connection to many failed read attempts");
            _client.stop();
            return;
        }

        if(_currentWritCommand != nullptr && !_readTimeout.has_value() && !_writeTimeout.has_value()){
            _writeTimeout.emplace(COMMEND_TIMEOUT * 1000);

            ESP_LOGD(TAG,"send new write data request");

            //modbus frame
            std::string businessfield = std::string("0110") + _currentWritCommand->writeRegister.c_str() + "0001" + DeyeUtils::lengthToString(_currentWritCommand->length,2).c_str() + _currentWritCommand->valueToWrite.c_str();
            std::string crc = DeyeUtils::modbusCRC16FromASCII(businessfield);

            std::string command = createReqeustDataCommand(DeyeUtils::hex_to_bytes(businessfield + crc));

            _client.write(command.c_str(),command.length());
            ConnectionStatistics.SendWriteDataRequests++;

            _systemConfigParaParser->setLastLimitRequestSuccess(LastCommandSuccess::CMD_PENDING);
        }
    }
}

void CustomModbusDeyeInverter::hostOrPortUpdated() {
    _client.stop();
}

std::string CustomModbusDeyeInverter::createReqeustDataCommand(const std::string & modbusFrame) {

    // Constants in flash - zero heap allocation
    static constexpr char START_BYTE = '\xA5';
    static constexpr char CONTROL_CODE[] = {'\x10', '\x45'};
    static constexpr char SERIAL_FILL[] = {'\x00', '\x00'};
    static constexpr char DATA_FIELD[] = {'\x02','\x00','\x00','\x00','\x00','\x00','\x00',
                                          '\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00'};
    static constexpr char END_CODE = '\x15';

    // Compute length field (little-endian 16-bit): total = 13 + modbusFrame.size() + 2
    uint16_t frameLen = static_cast<uint16_t>(13 + modbusFrame.size() + 2);
    char lengthBytes[2];
    lengthBytes[0] = static_cast<char>(frameLen & 0xFF);        // low byte first (little-endian)
    lengthBytes[1] = static_cast<char>((frameLen >> 8) & 0xFF); // high byte second

    // Compute serial number bytes (4 bytes, little-endian from hex representation)
    char hexStr[17];
    snprintf(hexStr, sizeof(hexStr), "%08llx", std::strtoull(serialString().c_str(), nullptr, 10));

    // Convert hex pairs to bytes in reversed order (little-endian)
    char serialBytes[4];
    for (int i = 0; i < 4; i++) {
        char pair[3] = { hexStr[6 - i*2], hexStr[7 - i*2], '\0' };
        serialBytes[i] = static_cast<char>(std::strtoul(pair, nullptr, 16));
    }

    // Single heap allocation - reserve exact size needed
    // 1(start) + 2(len) + 2(ctrl) + 2(serial_fill) + 4(serial) + 15(datafield) + modbusFrame + 1(checksum) + 1(end)
    std::string frame;
    frame.reserve(28 + modbusFrame.size());

    frame += START_BYTE;
    frame.append(lengthBytes, 2);
    frame.append(CONTROL_CODE, 2);
    frame.append(SERIAL_FILL, 2);
    frame.append(serialBytes, 4);
    frame.append(DATA_FIELD, 15);
    frame += modbusFrame;

    // Compute checksum over all bytes except the start byte
    uint8_t check = 0;
    for (size_t i = 1; i < frame.size(); i++) {
        check += static_cast<uint8_t>(frame[i]);
    }
    frame += static_cast<char>(check);
    frame += END_CODE;

    return frame;
}

void CustomModbusDeyeInverter::onDataReceived(void *data, size_t len) {
    if(_redBytes.load(std::memory_order_acquire) > 0) {
        ESP_LOGW(TAG,"Deye Custom Modbus -> Data dropped, previous data not yet consumed (%d bytes)", len);
        return;
    }
    if(len > READ_BUFFER_LENGTH){
        ESP_LOGE(TAG,"Read buffer too short, not all data used!");
    }
    ESP_LOGD(TAG,"Deye Custom Modbus -> Received some data: %d",len);
    std::lock_guard<std::mutex> lock(_readDataLock);
    size_t useLen = std::min(len,(size_t)READ_BUFFER_LENGTH);
    memcpy(_readBuffer,data,useLen);
    _redBytes.store(useLen, std::memory_order_release);
}

bool CustomModbusDeyeInverter::isReachable() {
    return _client.connected();
}
void CustomModbusDeyeInverter::resetStats() {
    ConnectionStatistics = {};
}

bool CustomModbusDeyeInverter::supportsPowerDistributionLogic() {
    return false;
}

void CustomModbusDeyeInverter::onPollTimeChanged() {
    BaseInverter::onPollTimeChanged();
    _pollDataTimout.setTimeout(_pollTime * 1000);
}

void CustomModbusDeyeInverter::handleWriteResponse() {
    ESP_LOGD(TAG,"received wire data response");

    if (_redBytes.load(std::memory_order_relaxed) < 25 + 10) {
        ESP_LOGD(TAG,"write response not enough data -> skip");
        return;
    }

    const uint8_t* frame = reinterpret_cast<const uint8_t*>(_readBuffer + 25);

    // Check for write response function code: 0x01 0x10
    if (frame[0] != 0x01 || frame[1] != 0x10) {
        ESP_LOGD(TAG,"write response not a valid write response -> skip");
        return;
    }

    // Compare register address (bytes 2-3) against expected write register
    // _currentWritCommand->writeRegister is a 4-char hex string like "0028"
    char regHex[5];
    snprintf(regHex, sizeof(regHex), "%02x%02x", frame[2], frame[3]);
    if (strncmp(regHex, _currentWritCommand->writeRegister.c_str(), 4) != 0) {
        ESP_LOGD(TAG,"write response not same register as written -> skip");
        return;
    }

    // Verify CRC: compute over first 6 bytes of modbus frame, compare with bytes 6-7
    uint16_t computedCrc = crc16(frame, 6);
    uint16_t receivedCrc = static_cast<uint16_t>(frame[6]) | (static_cast<uint16_t>(frame[7]) << 8);

    ESP_LOGD(TAG,"compare crcs: %04x -> %04x", receivedCrc, computedCrc);

    if (computedCrc != receivedCrc) {
        ESP_LOGI(TAG,"write crc not correct, failed");

        _systemConfigParaParser->setLastLimitRequestSuccess(LastCommandSuccess::CMD_NOK);
        //no return still reset data with error
    } else {
        if (frame[2] == 0x00 && frame[3] == 0x28) {
            char * p;
            float val = (float)std::strtoul(_currentWritCommand->valueToWrite.c_str(), &p, 16);
            _systemConfigParaParser->setLimitPercent(val);
            ESP_LOGI(TAG,"successfully set new limit %f", val);
        } else if (frame[2] == 0x00 && frame[3] == 0x2B) {
            char * p;
            uint32_t val = std::strtoul(_currentWritCommand->valueToWrite.c_str(), &p, 16);
            ESP_LOGI(TAG, "successfully set on/off flag %d", val);
        } else {
            ESP_LOGI(TAG, "received write response to unknown register");
        }

        ConnectionStatistics.SuccessfulWriteDataRequests++;

        _systemConfigParaParser->setLastLimitRequestSuccess(LastCommandSuccess::CMD_OK);
    }

    _writeTimeout.reset();
    _currentWritCommand = nullptr;
}

void CustomModbusDeyeInverter::handleReadResponse() {
    _readTimeout.reset();
    size_t redBytes = _redBytes.load(std::memory_order_relaxed);
    if (redBytes < 25 + 156 + 4) {
        ESP_LOGD(TAG, "skip response not enough data: %d", redBytes);
        return;
    }

    const int headerOffset = 25;
    const uint8_t* frame = reinterpret_cast<const uint8_t*>(_readBuffer + headerOffset);
    size_t frameLen = redBytes - headerOffset - 4; // exclude wrapper: 25 header + 2 crc + 1 checksum + 1 end

    // Check for read response function code: 0x01 0x03
    if (frame[0] != 0x01 || frame[1] != 0x03) {
        ESP_LOGD(TAG, "read response not a valid read response -> skip");
        return;
    }

    // Verify CRC: computed over frame bytes, compared with 2 bytes after frame
    const uint8_t* crcBytes = reinterpret_cast<const uint8_t*>(_readBuffer + headerOffset + frameLen);
    uint16_t computedCrc = crc16(frame, static_cast<uint8_t>(frameLen));
    uint16_t receivedCrc = static_cast<uint16_t>(crcBytes[0]) | (static_cast<uint16_t>(crcBytes[1]) << 8);

    ESP_LOGD(TAG, "compare crcs: %04x -> %04x", receivedCrc, computedCrc);

    if (computedCrc != receivedCrc) {
        ESP_LOGI(TAG, "read crc not correct, failed");
    } else {
        int i = headerOffset + 1;

        //swap low and height from 4 byte numbers
        swapTwoBytes(_readBuffer, i + 40 + 20);
        swapTwoBytes(_readBuffer, i + 40 + 24);
        swapTwoBytes(_readBuffer, i + 40 + 30);
        swapTwoBytes(_readBuffer, i + 40 + 36);
        swapTwoBytes(_readBuffer, i + 40 + 8);

        _statisticsParser->beginAppendFragment();
        _statisticsParser->clearBuffer();
        _statisticsParser->appendFragment(0, (uint8_t *) _readBuffer + i + 40, 112);
        _statisticsParser->resetRxFailureCount();
        _statisticsParser->endAppendFragment();

        handleDeyeDayCorrection();

        _statisticsParser->setLastUpdate(millis());

        ConnectionStatistics.SuccessfulReadDataRequests++;

        _systemConfigParaParser->setLimitPercent(DeyeUtils::defaultParseFloat(i + 2 , (uint8_t *) _readBuffer));

        ESP_LOGD(TAG, "handled new valid read data");
    }

    _readTimeout.reset();
}

String CustomModbusDeyeInverter::LogTag() {
    return TAG;
}
