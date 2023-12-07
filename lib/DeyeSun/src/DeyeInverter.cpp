#include "DeyeInverter.h"

#include <cstring>
#include <sstream>
#include <ios>
#include <iomanip>

const std::vector<RegisterMapping> DeyeInverter::_registersToRead = {
        RegisterMapping("006D",1,2),
        RegisterMapping("006F",1,6),
        RegisterMapping("006E",1,4),
        RegisterMapping("0070",1,8),//TODO calculate ampere
        RegisterMapping("003C",1,34),
        RegisterMapping("0041",1,14),
        RegisterMapping("0042",1,16),
        RegisterMapping("003F",2,36),
        RegisterMapping("0045",1,10),
        RegisterMapping("0047",1,12),
        RegisterMapping("0028",1,24),
        RegisterMapping("0049",1,18),
        RegisterMapping("004C",1,26),
        RegisterMapping("004F",1,20),
        RegisterMapping("003B",1,32),
        RegisterMapping("0056",2,22),
        RegisterMapping("005A",1,30),
        RegisterMapping("0010",1,24)
};

static const byteAssign_t byteAssignment[] = {
        //type, channel, field,  uint, first, byte in buffer, number of bytes in buffer, divisor, isSigned; // allow negative numbers, digits; // number of valid digits after the decimal point
        { TYPE_DC, CH0, FLD_UDC, UNIT_V, 2, 2, 10, true, 1 },
        { TYPE_DC, CH0, FLD_IDC, UNIT_A, 4, 2, 10, true, 2 },
        //{ TYPE_DC, CH0, FLD_PDC, UNIT_W, 6, 2, 10, false, 1 },
        { TYPE_DC, CH0, FLD_YD, UNIT_WH, 14, 2, 1, true, 0 },
        { TYPE_DC, CH0, FLD_YT, UNIT_KWH, 10, 2, 1000, true, 3 },
        { TYPE_DC, CH0, FLD_IRR, UNIT_PCT, CALC_IRR_CH, CH0, CMD_CALC, false, 3 },

        { TYPE_DC, CH1, FLD_UDC, UNIT_V, 6, 2, 10, true, 1 },
        { TYPE_DC, CH1, FLD_IDC, UNIT_A, 8, 2, 100, true, 2 },
        //{ TYPE_DC, CH1, FLD_PDC, UNIT_W, 12, 2, 10, false, 1 },
        { TYPE_DC, CH1, FLD_YD, UNIT_WH, 16, 2, 1, true, 0 },
        { TYPE_DC, CH1, FLD_YT, UNIT_KWH, 12, 2, 1000, true, 3 },
        { TYPE_DC, CH1, FLD_IRR, UNIT_PCT, CALC_IRR_CH, CH1, CMD_CALC, false, 3 },

        { TYPE_AC, CH0, FLD_UAC, UNIT_V, 18, 2, 10, true, 1 },
        { TYPE_AC, CH0, FLD_IAC, UNIT_A, 26, 2, 10, true, 2 },
        { TYPE_AC, CH0, FLD_PAC, UNIT_W, 22, 4, 10, false, 1 },
        { TYPE_AC, CH0, FLD_Q, UNIT_VAR, 24, 2, 10, true, 1 },//rated power
        { TYPE_AC, CH0, FLD_F, UNIT_HZ, 20, 2, 100, true, 2 },
        { TYPE_AC, CH0, FLD_PF, UNIT_NONE, 28, 2, 1000, true, 3 },//todo calculate

        { TYPE_INV, CH0, FLD_T, UNIT_C, 30, 2, 10, true, 1 },
        { TYPE_INV, CH0, FLD_EVT_LOG, UNIT_NONE, 32, 2, 1, true, 0 },//current status

        //{ TYPE_AC, CH0, FLD_YD, UNIT_WH, CALC_YD_CH0, 0, CMD_CALC, false, 0 },
        { TYPE_DC, CH1, FLD_YD, UNIT_WH, 34, 2, 1, true, 0 },
        //{ TYPE_AC, CH0, FLD_YT, UNIT_KWH, CALC_YT_CH0, 0, CMD_CALC, false, 3 },
        { TYPE_AC, CH0, FLD_YT, UNIT_KWH, 36, 4, 1000, true, 3 },
        { TYPE_AC, CH0, FLD_PDC, UNIT_W, CALC_PDC_CH0, 0, CMD_CALC, true, 1 },
        { TYPE_AC, CH0, FLD_EFF, UNIT_PCT, CALC_EFF_CH0, 0, CMD_CALC, true, 3 }
};

unsigned DeyeInverter::hex_char_to_int( char c ) {
    unsigned result = -1;
    if( ('0' <= c) && (c <= '9') ) {
        result = c - '0';
    }
    else if( ('A' <= c) && (c <= 'F') ) {
        result = 10 + c - 'A';
    }
    else if( ('a' <= c) && (c <= 'f') ) {
        result = 10 + c - 'a';
    }
    else {
        Serial.print("could not convert to hex : ");
        Serial.print(c);
        Serial.print(" decimal:");
        Serial.println((uint8_t)c);
        assert( 0 );
    }
    return result;
}

unsigned short DeyeInverter::modbusCRC16FromHex(const String & message)
{
    const unsigned short generator = 0xA001;
    unsigned short crc = 0xFFFF;
    for(int i = 0; i < message.length(); ++i)
    {
        crc ^= (unsigned short)message[i];
        for(int b = 0; b < 8; ++b)
        {
            if((crc & 1) != 0)
            {
                crc >>= 1;
                crc ^= generator;
            }
            else
                crc >>= 1;

        }
    }
    return crc;
}

String DeyeInverter::modbusCRC16FromASCII(const String & input) {

    Serial.print("Calculating crc for: ");
    Serial.println(input);

    String hexString;

    for(int i=0;i<input.length();i=i+2){
        unsigned number = hex_char_to_int( input[ i ] ); // most signifcnt nibble
        unsigned lsn = hex_char_to_int( input[ i + 1 ] ); // least signt nibble
        number = (number << 4) + lsn;
        hexString += (char)number;
    }

    unsigned short res = modbusCRC16FromHex(hexString);

    std::stringstream stream;
    stream << std::setw(4) << std::setfill('0')  <<std::hex << static_cast<uint16_t >(res);
    std::string result( stream.str() );

    return String()+result[2]+result[3]+result[0]+result[1];
}


DeyeInverter::DeyeInverter(uint64_t serial):
_socket(nullptr){
    _serial = serial;
    _serialString = String(serial);

    _alarmLogParser.reset(new DeyeAlarmLog());
    _devInfoParser.reset(new DeyeDevInfo());
    _gridProfileParser.reset(new DeyeGridProfile());
    _powerCommandParser.reset(new DeyePowerCommand());
    _statisticsParser.reset(new StatisticsParser());
    _systemConfigParaParser.reset(new DeyeSystemConfigPara());

    _statisticsParser->setByteAssignment(byteAssignment,sizeof(byteAssignment) / sizeof(byteAssignment[0]));

    _commandPosition = 0;
}

void DeyeInverter::sendSocketMessage(String message) {

    Serial.println("Sending deye message: "+message);

    IPAddress RecipientIP(192, 168, 1, 138);
    _socket->beginPacket(RecipientIP, 48899);
    _socket->print(message);
    _socket->endPacket();

    _lastSuccessfullPoll = millis();
}


void DeyeInverter::updateSocket() {

    /*
                inv->Statistics()->beginAppendFragment();
                inv->Statistics()->clearBuffer();
                inv->Statistics()->appendFragment(0,toAdd,4);
                inv->Statistics()->endAppendFragment();
                inv->Statistics()->resetRxFailureCount();
                inv->Statistics()->setLastUpdate(millis());
     */


    //no new data required
    if (millis() - _lastSuccessData < (10 * 1000)) {
        return;
    }

    if (!WiFi.isConnected()) {
        Serial.println("Wifi not connected");
        _socket = nullptr;
        return;
    }

    if (_socket == nullptr) {
        Serial.println("New connection");
        _socket = std::make_unique<WiFiUDP>();
        sendSocketMessage("WIFIKIT-214028-READ");
        _commandPosition = 0;
        _lastPoll = millis();
        _lastSuccessfullPoll = millis();
    }

    int packetSize = _socket->parsePacket();
    if (packetSize > 0){
        _lastSuccessfullPoll = millis();
        Serial.println("Recevied new package");
        _lastPoll = millis();
        size_t num = _socket->read(_readBuff,packetSize);
        _socket->flush();
        Serial.println(num);
        if(_commandPosition == 0){
            if(!parseInitInformation(num)){
                _socket = nullptr;
                _lastSuccessfullPoll = millis();
                return;
            }
            //TOD0 handle register stuff
            sendSocketMessage("+ok");
            _commandPosition++;
            sendCurrentRegisterRead();

            _statisticsParser->beginAppendFragment();
            _statisticsParser->clearBuffer();
        }else{
            int ret = handleRegisterRead(num);
            if(ret == 0){//ok
                if(_commandPosition >= _registersToRead.size()){
                    sendSocketMessage("AT+Q\n");
                    //TODO with commandlist
                    _socket = nullptr;
                    _lastSuccessData = millis();

                    _statisticsParser->endAppendFragment();
                    _statisticsParser->setLastUpdate(millis());

                    Serial.println("Red succesfull all values");
                    return;
                }
                _commandPosition++;
                //pollWait = true;
                sendCurrentRegisterRead();
            }else if(ret == -1){//try again error
                //pollWait = true;
                sendCurrentRegisterRead();
            }else{
                _socket = nullptr;

                _statisticsParser->incrementRxFailureCount();
                _statisticsParser->endAppendFragment();
                _statisticsParser->setLastUpdate(millis());

                return;
            }
        }
    }

    //timeout of one second
    if (millis() - _lastPoll > (1 * 1000)) {
        _lastPoll = millis();
        Serial.println("Max poll time overtook try again");
        if (_commandPosition == 0) {
            sendSocketMessage("WIFIKIT-214028-READ");
        } else {
            if (_commandPosition == 1) {
                sendSocketMessage("+ok");
            }
            sendCurrentRegisterRead();
        }
    }

    //timeout of one second
    if (millis() - _lastSuccessfullPoll > (10 * 1000)) {
        Serial.println("Nothing received over 10 sec reset connection");
        _socket = nullptr;
        _statisticsParser->endAppendFragment();
    }
}

uint64_t DeyeInverter::serial() {
    return _serial;
}

String DeyeInverter::typeName() {
    return "DeyeSun";
}

bool DeyeInverter::isProducing() {
    return false;
}

bool DeyeInverter::isReachable() {
    return false;
}

bool DeyeInverter::sendActivePowerControlRequest(float limit, PowerLimitControlType type) {
    return false;
}

bool DeyeInverter::resendPowerControlRequest() {
    return false;
}

bool DeyeInverter::sendRestartControlRequest() {
    return false;
}

bool DeyeInverter::sendPowerControlRequest(bool turnOn) {
    return false;
}

void DeyeInverter::setHostnameOrIp(const char *hostOrIp) {
    uint8_t len = strlen(hostOrIp);
    if (len + 1 > MAX_NAME_HOST) {
        len = MAX_NAME_HOST - 1;
    }
    strncpy(_name, _hostnameOrIp, len);
    _name[len] = '\0';
}

void DeyeInverter::setPort(uint16_t port) {
    _port = port;
}

bool DeyeInverter::parseInitInformation(size_t length) {
    String ret = String(_readBuff,length);
    Serial.print("Recevied Initial Read: ");
    Serial.println(ret);
    return true;
}

int DeyeInverter::handleRegisterRead(size_t length) {
    Serial.print("Recevied Register Read: ");
    Serial.println(_readBuff);
    String ret;
    for(int i=0;i<length;i++){
        //there are those od characters in response filter them
        if(_readBuff[i] >= 32){
            ret+=_readBuff[i];
        }
    }

    Serial.print("Fileted recevied Register Read: ");
    Serial.println(ret);

    if(ret.startsWith("+ERR=")) {
        if (ret.startsWith("+ERR=-1")) {
            return -1;
        }
        return -2;
    }

    if (std::count(ret.begin(), ret.end(),'.') > 0){
        Serial.print("Received wrong message");
        return -1;
    }

    auto & current = _registersToRead[_commandPosition-1];


    //+ok= plus first 6 header characters
    int start = 4 + 6;

    if(ret.length() < start+(current.length*2)){
        Serial.print("Error while reading data not entoh data on: ");
        Serial.println(current.readRegister);
        return -1000;
    }

    //String hexString = ret.substring(start,start+current.length);
    String hexString;

    for(int i=start;i<start+(current.length*4);i=i+2){

        String test;

        test+=ret[i];
        test+=ret[i+1];

        Serial.print("COnverting: ");
        Serial.println(test);

        unsigned number = hex_char_to_int( ret[ i ] ); // most signifcnt nibble
        unsigned lsn = hex_char_to_int( ret[ i + 1 ] ); // least signt nibble
        number = (number << 4) + lsn;
        hexString += (char)number;
    }

    if(current.length == 2){
        hexString = hexString.substring(0,3)+hexString.substring(4);
    }

    _statisticsParser->appendFragment(current.targetPos,(uint8_t*)(hexString.c_str()),current.length*2);//i know this is string cast is ugly

    return 0;
}

void DeyeInverter::sendCurrentRegisterRead() {
    auto & current = _registersToRead[_commandPosition-1];
    String data = "0103";
    data += current.readRegister;

    std::stringstream ss;
    ss << std::setw(4) << std::setfill('0') << (int)current.length;
    data += ss.str().c_str();

    String checksum = modbusCRC16FromASCII(data);

    sendSocketMessage("AT+INVDATA=8,"+data+checksum+"\n");
}
