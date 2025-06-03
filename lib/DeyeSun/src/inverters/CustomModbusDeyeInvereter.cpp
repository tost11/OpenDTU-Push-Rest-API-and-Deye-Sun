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
    createReqeustDataCommand();

    _client.onData([&](void * arg, AsyncClient * client,void *data, size_t len){this->onDataReceived(data,len);});
    _redBytes = 0;
}

deye_inverter_type CustomModbusDeyeInverter::getDeyeInverterType() const {
    return Deye_Sun_Custom_Modbus;
}

void CustomModbusDeyeInverter::update() {

    if(_statusPrintTimeout.occured()){
        MessageOutput.printf("Deye Custom Modbus -> Socket status: %s\n",_client.stateToString());
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

    if(_client.connected()){
        if(!getEnablePolling()){
            MessageOutput.println("Deye Custom Modbus -> stop polling data");
            _client.stop();
        }

        //handle data fetching
        if(_redBytes > 0) {
            std::lock_guard<std::mutex> lock(_readDataLock);
            if(_redBytes < 27 + 112 + 6) {
                MessageOutput.printf("Deye Custom Modbus -> skip response not enough data: %d\n",_redBytes);
                //TODO some more error handling
            }else{
                MessageOutput.println("Deye Custom Modbus -> handled new valid data");
                _statisticsParser->beginAppendFragment();
                _statisticsParser->clearBuffer();
                _statisticsParser->appendFragment(27,(uint8_t *)_readBuffer,114);
                _statisticsParser->setLastUpdate(millis());
                _statisticsParser->resetRxFailureCount();
                _statisticsParser->endAppendFragment();
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
    }

    if(_client.state() == 4){//establised
        if(_requestDataTimeout.occured()){
            MessageOutput.println("Deye Custom Modbus -> send new data request");
            _requestDataTimeout.reset();
            _client.write(_requestDataCommand,SEND_REQUEST_BUFFER_LENGTH);
        }
    }

}

void CustomModbusDeyeInverter::hostOrPortUpdated() {
    _client.stop();
}

void CustomModbusDeyeInverter::createReqeustDataCommand() {

    int start_register = 60;
    int end_register = 116;
    std::string start = DeyeUtils::hex_to_bytes("A5");
    std::string length = DeyeUtils::hex_to_bytes("1700");
    std::string controlcode = DeyeUtils::hex_to_bytes("1045");
    std::string serialFill = DeyeUtils::hex_to_bytes("0000");
    std::string datafield = DeyeUtils::hex_to_bytes("020000000000000000000000000000");
    //std::string pos_ini = "003B";
    std::string pos_ini = DeyeUtils::lengthToHexString(start_register,4).c_str();
    std::string pos_fin = DeyeUtils::lengthToHexString(end_register - start_register + 1,4).c_str();
    std::string businessfield = "0103" + pos_ini + pos_fin;
    std::string crc = DeyeUtils::hex_to_bytes(DeyeUtils::modbusCRC16FromASCII(businessfield));
    std::string checksum = DeyeUtils::hex_to_bytes("00");
    std::string endCode = DeyeUtils::hex_to_bytes("15");
    // check for the presence of the shield:
    //std::string snHex = "F56E3BEA"
    //std::string snHex = "EA3B6EF5";
    //uint64_t ser = serial();
    //std::string snHex = std::string((char*)&ser,8);
    MessageOutput.printf("Serial is: %llu\n",serial());
    MessageOutput.printf("Serial is: %s\n",serialString().c_str());
    //MessageOutput.printf("Hex Serial is: %s\n",snHex.c_str());
    std::string inverter_sn2 = DeyeUtils::hex_to_bytes("EA3B6EF5");

    std::string frame = start + length  + controlcode + serialFill + inverter_sn2 + datafield + DeyeUtils::hex_to_bytes(businessfield) + crc + checksum + endCode;
    //std::string frame = start + length + controlcode;

    uint16_t lenToSend = frame.length();

    MessageOutput.printf("Len to send: %d\n",lenToSend);

    assert(SEND_REQUEST_BUFFER_LENGTH >= lenToSend);

    memcpy(_requestDataCommand, frame.c_str(),lenToSend);

    //calculate crc and set
    int check = 0;
    for(int i=1;i<frame.length() - 2;i++){
        check += _requestDataCommand[i] & 255;
    }
    _requestDataCommand[frame.length() - 2] = int((check & 255));

    std::string test;
    for(int i=0;i<frame.length();i++){
        test += _requestDataCommand[i];
    }

    MessageOutput.println("start --------");
    for(int i=0;i<test.length();i++){
        MessageOutput.println((int)test[i]);
    }
    MessageOutput.println("end --------");
}

void CustomModbusDeyeInverter::onDataReceived(void *data, size_t len) {
    if(len < 27){
        MessageOutput.printf("Deye Custom Modbus -> not received enough data skip it for now, length: %d\n",len);
        return;
    }
    if(len > READ_BUFFER_LENGTH){
        MessageOutput.println("Deye Custom Modbus -> Read buffer to short not all data used!");
    }
    MessageOutput.println("Deye Custom Modbus -> Received some data");
    std::lock_guard<std::mutex> lock(_readDataLock);
    _redBytes = std::min(len,(size_t)READ_BUFFER_LENGTH);
    memcpy(_readBuffer,data,std::min(len,_redBytes));
}

bool CustomModbusDeyeInverter::isReachable() {
    return _client.connected();
}

bool CustomModbusDeyeInverter::sendActivePowerControlRequest(float limit, const PowerLimitControlType type) {
    return false;
}

bool CustomModbusDeyeInverter::resendPowerControlRequest() {
    return false;
}

bool CustomModbusDeyeInverter::sendRestartControlRequest() {
    return false;
}

bool CustomModbusDeyeInverter::sendPowerControlRequest(const bool turnOn) {
    return false;
}

void CustomModbusDeyeInverter::resetStats() {

}

bool CustomModbusDeyeInverter::supportsPowerDistributionLogic() {
    return false;
}
