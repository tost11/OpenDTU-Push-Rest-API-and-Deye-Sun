//
// Created by lukas on 31.05.25.
//

#include "CustomModbusDeyeInverter.h"
#include "DeyeUtils.h"

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
        MessageOutput.printfDebug("Deye Custom Modbus -> Socket status: %s\n",_client.stateToString());
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

    //TODO think about better handling for this
    if(_currentWritCommand == nullptr){
        checkForNewWriteCommands();
    }

    if(_client.connected()){

        if(!getEnablePolling()){
            MessageOutput.println("Deye Custom Modbus -> stop polling data");
            _client.stop();
        }

        //handle data fetching
        if(_redBytes > 0) {



            if (_redBytes < 27) {
                MessageOutput.println("not enoth data");
                //not enoth data
                _writeTimeout = nullptr;
                _readTimeout = nullptr;
            } else if (_redBytes == 29) {
                MessageOutput.println("error response");
                //error
                _writeTimeout = nullptr;
                _readTimeout = nullptr;
            } else if (_redBytes < 29 + 4) {
                MessageOutput.println("not enoth data for valid frame");
                //not enoth data
                _writeTimeout = nullptr;
                _readTimeout = nullptr;
            } else if(_readBuffer[0] != 0xA5) {
                MessageOutput.println("start bytes wrong");
                _writeTimeout = nullptr;
                _readTimeout = nullptr;
            } else if(_readBuffer[_redBytes -1] != 0x15) {
                MessageOutput.println("end bytes wrong");
                _writeTimeout = nullptr;
                _readTimeout = nullptr;
            } else {


            /*
            if not frame:
# Error was already logged in `send_request()` function
            return None
            if len(frame) == 29:
            self.__parse_response_error_code(frame)
            return None
            if frame[0:3] == b"AT+":
            self.__log.error(
                    "AT response detected. Try switching to 'AT' protocol. "
                    "Set 'DEYE_LOGGER_PROTOCOL=at' and remove DEYE_LOGGER_PORT from your config"
            )
            return None
            if len(frame) < (29 + 4):
            self.__log.error("Response frame is too short")
            return None
            if frame[0] != 0xA5:
            self.__log.error("Response frame has invalid starting byte")
            return None
            if frame[-1] != 0x15:
            self.__log.error("Response frame has invalid ending byte")
            return None*/

                std::lock_guard<std::mutex> lock(_readDataLock);
                MessageOutput.printfDebug("Deye Custom Modbus -> Received bytes are: %d\n", _redBytes);
                if (_readTimeout != nullptr) {
                    _readTimeout = nullptr;
                    if (_redBytes < 27 + 152 + 6) {
                        MessageOutput.printf("Deye Custom Modbus -> skip response not enough data: %d\n", _redBytes);
                        //TODO some more error handling
                    } else {
                        //swap low and height from 4 byte numbers
                        swapTwoBytes(_readBuffer, 26 + 40 + 20);
                        swapTwoBytes(_readBuffer, 26 + 40 + 24);
                        swapTwoBytes(_readBuffer, 26 + 40 + 30);
                        swapTwoBytes(_readBuffer, 26 + 40 + 36);
                        swapTwoBytes(_readBuffer, 26 + 40 + 8);

                        _statisticsParser->beginAppendFragment();
                        _statisticsParser->clearBuffer();
                        _statisticsParser->appendFragment(0, (uint8_t *) _readBuffer + 26 + 40, 112);
                        _statisticsParser->resetRxFailureCount();
                        _statisticsParser->endAppendFragment();

                        handleDeyeDayCorrection();

                        _statisticsParser->setLastUpdate(millis());

                        ConnectionStatistics.SuccessfulReadDataRequests++;

                        _systemConfigParaParser->setLimitPercent(DeyeUtils::defaultParseFloat(28,(uint8_t*)_readBuffer));

                        MessageOutput.printlnDebug("Deye Custom Modbus -> handled new valid read data");
                    }
                } else if (_writeTimeout != nullptr) {
                    MessageOutput.println("Deye Custom Modbus -> receviced wirte data response");
                    _writeTimeout = nullptr;
                    _currentWritCommand = nullptr;

                    if (_redBytes < 27 + 6) {
                        MessageOutput.println("Deye Custom Modbus -> handled new valid write data");
                        //TODO implement
                    }

                } else {
                    MessageOutput.println("Deye Custom Modbus -> receviced data but no where requested...");
                }
            }
            _redBytes = 0;
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
        MessageOutput.printf("Deye Custom Modbus -> reconnect %s %d\n",address,_port);
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
            MessageOutput.print("Deye Custom Modbus -> read timout hit while waiting for data");
            _readTimeout = nullptr;
        }

        if(_writeTimeout != nullptr && _writeTimeout->occured()){
            MessageOutput.print("Deye Custom Modbus -> write timout hit while waiting for data");
            _writeTimeout = nullptr;
        }

        if(_requestDataTimeout.occured() && _writeTimeout == nullptr && _readTimeout == nullptr){
            _readTimeout = std::make_unique<TimeoutHelper>(COMMEND_TIMEOUT * 1000);

            //TODO check crc ok
            MessageOutput.println("Deye Custom Modbus -> send new read data request");
            _requestDataTimeout.reset();
            _client.write(_requestDataCommand.c_str(),_requestDataCommand.length());
            ConnectionStatistics.SendReadDataRequests++;
        }

        if(_currentWritCommand != nullptr && _readTimeout == nullptr && _writeTimeout == nullptr){
            _writeTimeout = std::make_unique<TimeoutHelper>(COMMEND_TIMEOUT * 1000);

            //TODO check crc ok
            MessageOutput.printfDebug("Deye Custom Modbus -> send new write data request");

            //modbus frame
            std::string businessfield = std::string("0110") + _currentWritCommand->writeRegister.c_str() + "0001" + DeyeUtils::lengthToString(_currentWritCommand->length,2).c_str() + _currentWritCommand->valueToWrite.c_str();
            std::string crc = DeyeUtils::modbusCRC16FromASCII(businessfield);

            std::string command = createReqeustDataCommand(DeyeUtils::hex_to_bytes(businessfield + crc));

            _client.write(command.c_str(),command.length());
            ConnectionStatistics.SendWriteDataRequests++;
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
        MessageOutput.println("Deye Custom Modbus -> Read buffer to short not all data used!");
    }
    MessageOutput.printlnDebug("Deye Custom Modbus -> Received some data");
    std::lock_guard<std::mutex> lock(_readDataLock);
    _redBytes = std::min(len,(size_t)READ_BUFFER_LENGTH);
    memcpy(_readBuffer,data,std::min(len,_redBytes));
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