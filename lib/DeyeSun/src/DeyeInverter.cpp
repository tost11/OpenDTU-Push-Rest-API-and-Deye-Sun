#include "DeyeInverter.h"

#include <cstring>
#include <sstream>
#include <ios>
#include <iomanip>

const std::vector<RegisterMapping> DeyeInverter::_registersToRead = {
        RegisterMapping("006D",1),
        RegisterMapping("006F",1),
        RegisterMapping("006E",1),
        RegisterMapping("0070",1),
        RegisterMapping("003C",1),
        RegisterMapping("0041",1),
        RegisterMapping("0042",1),
        RegisterMapping("003F",2),
        RegisterMapping("0028",1),
        RegisterMapping("0049",1),
        RegisterMapping("004C",1),
        RegisterMapping("004F",1),
        RegisterMapping("003B",1),
        RegisterMapping("0056",2),
        RegisterMapping("005A",1),
        RegisterMapping("0010",1)
};

static const byteAssign_t byteAssignment[] = {
        {TYPE_DC, CH0, FLD_UDC, UNIT_V, 0,  4, 10, true, 1}
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
            sendCurrentRegisterRead();
            _commandPosition++;
        }else{
            int ret = handleRegisterRead(num);
            if(ret == 0){//ok
                if(_commandPosition+2 >= _registersToRead.size()){
                    sendSocketMessage("AT+Q\n");
                    //TODO with commandlist
                    _socket = nullptr;
                    _lastSuccessData = millis();
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
    String ret = String(_readBuff,length);
    Serial.print("Recevied Register Read: ");
    Serial.println(ret);
    if(ret.startsWith("+ERR=")) {
        if (ret.startsWith("+ERR=-1")) {
            return -1;
        }
        return -2;
    }
    return 0;
}

void DeyeInverter::sendCurrentRegisterRead() {
    auto & current = _registersToRead[_commandPosition+1];
    String data = "0103";
    data += current.readRegister;

    std::stringstream ss;
    ss << std::setw(4) << std::setfill('0') << (int)current.length;
    data += ss.str().c_str();

    String checksum = modbusCRC16FromASCII(data);

    sendSocketMessage("AT+INVDATA=8,"+data+checksum+"\n");
}
