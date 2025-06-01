//
// Created by lukas on 31.05.25.
//

#include "CustomModbusDeyeInvereter.h"
#include "DeyeUtils.h"

CustomModbusDeyeInvereter::CustomModbusDeyeInvereter(uint64_t serial):
DeyeInverter(serial){
    _reconnectTimeout.set(15);
    createReqeustDataCommand();
}

deye_inverter_type CustomModbusDeyeInvereter::getDeyeInverterType() const {
    return Deye_Sun_Custom_Modbus;
}

void CustomModbusDeyeInvereter::update() {

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
            _client.stop();
        }//here no else because disconnected means no data available so reed is needed to close socket
        //handle data fetching
    }



    //polling is disabled (night whatever) wait for existing socket connection and command if null not active skip check
    if(!_client.connected() && !getEnablePolling()){
        return;
    }


    if(!_client.connected() && _reconnectTimeout.occured()){
        _reconnectTimeout.zero();
        const char * addres = _resolvedIpByMacAdress == nullptr ? _oringalIpOrHostname.c_str() : _resolvedIpByMacAdress->c_str();
        _client.connect(addres, _port);

    }



}

void CustomModbusDeyeInvereter::hostOrPortUpdated() {
    _client.stop();
}

void CustomModbusDeyeInvereter::createReqeustDataCommand() {

    int start_register = 60;
    int end_register = 116;
    std::string start = DeyeUtils::hex_to_bytes("A5");
    std::string length = DeyeUtils::hex_to_bytes("1700");
    std::string controlcode = DeyeUtils::hex_to_bytes("1045");
    std::string serial = DeyeUtils::hex_to_bytes("0000");
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
    std::string snHex = "EA3B6EF5";
    std::string inverter_sn2 = DeyeUtils::hex_to_bytes(snHex);

    std::string frame = start + length  + controlcode + serial + inverter_sn2 + datafield + DeyeUtils::hex_to_bytes(businessfield) + crc + checksum + endCode;
    //std::string frame = start + length + controlcode;

    uint16_t lenToSend = frame.length();

    assert(SEND_REQUEST_BUFFER_LENGTH >= lenToSend);

    memcpy(_requestDataCommand, frame.c_str(),lenToSend);

    //calculate crc and set
    int check =0;
    for(int i=1;i<frame.length() - 2;i++){
        check += _requestDataCommand[i] & 255;
    }
    _requestDataCommand[frame.length() - 2] = int((check & 255));
}
