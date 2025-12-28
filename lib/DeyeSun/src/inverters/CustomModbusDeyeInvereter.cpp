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

    _client.onData([&](void * arg, AsyncClient * client,void *data, size_t len){this->onDataReceived(data,len);});
    _redBytes = 0;

    _wasConnecting = false;
}

CustomModbusDeyeInverter::~CustomModbusDeyeInverter() {
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
    getEventLog()->checkErrorsForTimeout();

    // Check and fetch firmware version periodically
    checkAndFetchFirmwareVersion();

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
        if(_redBytes > 0) {
            std::lock_guard<std::mutex> lock(_readDataLock);
            if (_redBytes < 27) {
                ESP_LOGD(TAG,"not enough data");
                //not enoth data
                _writeTimeout = nullptr;
                _readTimeout = nullptr;
            } else if (_redBytes == 29) {
                //TODO handle
                ESP_LOGD(TAG,"error response");
                //error
                _writeTimeout = nullptr;
                _readTimeout = nullptr;
            } else if (_redBytes < 29 + 4) {
                ESP_LOGD(TAG,"not enoth data for valid frame");
                //not enoth data
                _writeTimeout = nullptr;
                _readTimeout = nullptr;
            } else if(_readBuffer[0] != 0xA5) {
                ESP_LOGD(TAG,"start bytes wrong");
                _writeTimeout = nullptr;
                _readTimeout = nullptr;
            } else if(_readBuffer[_redBytes -1] != 0x15) {
                ESP_LOGD(TAG,"end bytes wrong");
                _writeTimeout = nullptr;
                _readTimeout = nullptr;
            } else {
                ESP_LOGD(TAG,"Received bytes are: %d\n", _redBytes);
                if (_readTimeout != nullptr) {
                    handleReadResponse();
                } else if (_writeTimeout != nullptr) {
                    handleWriteResponse();
                } else {
                    ESP_LOGD(TAG,"received data but no where requested...");
                }
            }
            _redBytes = 0;
        }
    }

    if(_client.state() != 4) {//not connected
        if(_currentWritCommand != nullptr || _limitToSet != nullptr || _powerTargetStatus){

            if(_currentWritCommand->writeRegister == "0028" || _limitToSet != nullptr){
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
            _writeTimeout = nullptr;
            _readTimeout = nullptr;
        }

        if(_readTimeout != nullptr && _readTimeout->occured()){
            ESP_LOGW(TAG,"read timout hit while waiting for data");
            _readTimeout = nullptr;
        }

        if(_writeTimeout != nullptr && _writeTimeout->occured()){
            ESP_LOGW(TAG,"write timout hit while waiting for data");
            _writeTimeout = nullptr;
            _systemConfigParaParser->setLastLimitRequestSuccess(LastCommandSuccess::CMD_NOK);
        }

        if(_requestDataTimeout.occured() && _writeTimeout == nullptr && _readTimeout == nullptr){
            _readTimeout = std::make_unique<TimeoutHelper>(COMMEND_TIMEOUT * 1000);

            ESP_LOGD(TAG,"end new read data request");
            _requestDataTimeout.reset();
            _client.write(_requestDataCommand.c_str(),_requestDataCommand.length());
            ConnectionStatistics.SendReadDataRequests++;
        }

        if(_currentWritCommand != nullptr && _readTimeout == nullptr && _writeTimeout == nullptr){
            _writeTimeout = std::make_unique<TimeoutHelper>(COMMEND_TIMEOUT * 1000);

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

    std::string start = DeyeUtils::hex_to_bytes("A5");

    //TODO find better way to do this
    std::string tmpLength = DeyeUtils::lengthToHexString(13+modbusFrame.size()+2,4);
    std::string length = DeyeUtils::hex_to_bytes(std::string()+tmpLength[2]+tmpLength[3]+tmpLength[0]+tmpLength[1]);
    std::string controlcode = DeyeUtils::hex_to_bytes("1045");
    std::string serialFill = DeyeUtils::hex_to_bytes("0000");
    std::string datafield = DeyeUtils::hex_to_bytes("020000000000000000000000000000");

    //modbus frame
    //std::string pos_ini = DeyeUtils::lengthToHexString(start_register,4).c_str();
    //std::string pos_fin = DeyeUtils::lengthToHexString(end_register - start_register + 1,4).c_str();
    //std::string businessfield = "0103" + pos_ini + pos_fin;
    //std::string crc = DeyeUtils::hex_to_bytes(DeyeUtils::modbusCRC16FromASCII(businessfield));

    std::string checksum = DeyeUtils::hex_to_bytes("00");
    std::string endCode = DeyeUtils::hex_to_bytes("15");

    //TODO find better way to do this
    char hexStr[9];
    sprintf(hexStr, "%08llx", std::strtoull(serialString().c_str(),0,10));
    //MessageOutput.printf("Hex value 1: %s\n",hexStr);
    std::string inverter_sn2 = "        ";
    inverter_sn2[0] = hexStr[6];
    inverter_sn2[1] = hexStr[7];
    inverter_sn2[2] = hexStr[4];
    inverter_sn2[3] = hexStr[5];
    inverter_sn2[4] = hexStr[2];
    inverter_sn2[5] = hexStr[3];
    inverter_sn2[6] = hexStr[0];
    inverter_sn2[7] = hexStr[1];

    std::string frame = start + length  + controlcode + serialFill + DeyeUtils::hex_to_bytes(inverter_sn2) + datafield + modbusFrame;// + checksum + endCode;
    int32_t check = 0;
    for(int i=1;i<frame.length();i++){
        check += frame[i] & 255;
    }

    char c = int((check & 255));
    frame.append(&c,1);
    frame += endCode;

/*
    memcpy(_requestDataCommand, frame.c_str(),lenToSend);

    //calculate crc and set
    int check = 0;
    for(int i=1;i<frame.length() - 2;i++){
        check += _requestDataCommand[i] & 255;
    }
    _requestDataCommand[frame.length() - 2] = int((check & 255));
*/

/*
    MessageOutput.println("start --------");
    for(int i=0;i<frame.length();i++){
        MessageOutput.println((int)frame[i]);
    }
    MessageOutput.println("end --------");
*/
    return frame;
}

void CustomModbusDeyeInverter::onDataReceived(void *data, size_t len) {
    if(len > READ_BUFFER_LENGTH){
        ESP_LOGE(TAG,"Read buffer to short not all data used!");
    }
    ESP_LOGD(TAG,"Deye Custom Modbus -> Received some data: %d",len);
    std::lock_guard<std::mutex> lock(_readDataLock);
    size_t useLen = std::min(len,(size_t)READ_BUFFER_LENGTH);
    memcpy(_readBuffer,data,useLen);
    _redBytes = useLen;
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

    if (_redBytes < 25 + 10) {
        ESP_LOGD(TAG,"write response not enough data -> skip");
        return;
    }

    int i=25;

    std::string frame = std::string(_readBuffer + i,6);
    frame = DeyeUtils::bytes_to_hex(frame);

    ESP_LOGD(TAG,"frame is: %s\n",frame.c_str());

    if(frame.substr(0,4) != "0110"){
        //not a write response
        ESP_LOGD(TAG,"write response not a valid write response -> skip");
        return;
    }

    if(frame.substr(4,4) != _currentWritCommand->writeRegister.c_str()){
        //not a write response
        ESP_LOGD(TAG,"write response not same register as written -> skip");
        return;
    }

    std::string isCrc = std::string(_readBuffer + i + 6,2);
    isCrc = DeyeUtils::bytes_to_hex(isCrc);

    std::string calcCrc = DeyeUtils::modbusCRC16FromASCII(frame);
    ESP_LOGD(TAG,"compare crcs: %s -> %s\n", isCrc.c_str(),calcCrc.c_str());

    if(isCrc != calcCrc){
        ESP_LOGI(TAG,"write crc not correct, failed");

        _systemConfigParaParser->setLastLimitRequestSuccess(LastCommandSuccess::CMD_NOK);
        //no return still reset data with error
    }else{
        if(frame.substr(4,4) == "0028"){
            char * p;
            float val = (float)std::strtoul( _currentWritCommand->valueToWrite.c_str(), & p, 16 );
            _systemConfigParaParser->setLimitPercent(val);
            ESP_LOGI(TAG,"successfully set new limit %f\n",val);
        }else if(frame.substr(4,4) == "002B"){
            char * p;
            uint32_t val = std::strtoul( _currentWritCommand->valueToWrite.c_str(), & p, 16 );
            ESP_LOGI(TAG, "successfully set on/off flag %d\n",val);
        }else{
            ESP_LOGI(TAG, "received write response to unknown register");
        }

        ConnectionStatistics.SuccessfulWriteDataRequests++;

        _systemConfigParaParser->setLastLimitRequestSuccess(LastCommandSuccess::CMD_OK);
    }

    _writeTimeout = nullptr;
    _currentWritCommand = nullptr;
}

void CustomModbusDeyeInverter::handleReadResponse() {
    _readTimeout = nullptr;
    if (_redBytes < 25 + 156 + 4) {
        ESP_LOGD(TAG, "skip response not enough data: %d\n", _redBytes);
        return;
    }

    int i=25;

    std::string frame = std::string(_readBuffer + i,_redBytes - i - 4);
    frame = DeyeUtils::bytes_to_hex(frame);

    ESP_LOGD(TAG, "frame is: %s\n",frame.c_str());

    if(frame.substr(0,4) != "0103"){
        //not a write response
        ESP_LOGD(TAG, "write response not a valid write response -> skip");
        return;
    }

    std::string isCrc = std::string(_readBuffer + (_redBytes - 4) , 2);
    isCrc = DeyeUtils::bytes_to_hex(isCrc);

    std::string calcCrc = DeyeUtils::modbusCRC16FromASCII(frame);
    ESP_LOGD(TAG, "compare crcs: %s -> %s\n", isCrc.c_str(),calcCrc.c_str());

    if(isCrc != calcCrc){
        ESP_LOGI(TAG, "read crc not correct, failed");
    }else {

        i = i + 1;

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

    _readTimeout = nullptr;
}

String CustomModbusDeyeInverter::LogTag() {
    return TAG;
}
