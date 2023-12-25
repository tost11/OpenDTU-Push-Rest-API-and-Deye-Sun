#include "DeyeInverter.h"
#include "DeyeUtils.h"
#include "Dns.h"

#include <cstring>
#include <sstream>
#include <ios>
#include <iomanip>

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

    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             ((uint32_t)((serial >> 32) & 0xFFFFFFFF)),
             ((uint32_t)(serial & 0xFFFFFFFF)));
    _serialString = serial_buff;

    _alarmLogParser.reset(new DeyeAlarmLog());
    _devInfoParser.reset(new DeyeDevInfo());
    _gridProfileParser.reset(new DeyeGridProfile());
    _powerCommandParser.reset(new DeyePowerCommand());
    _statisticsParser.reset(new StatisticsParser());
    _systemConfigParaParser.reset(new SystemConfigParaParser());

    _devInfoParser->setMaxPowerDevider(10);

    _needInitData = true;
    _commandPosition = 0;

    _ipAdress = nullptr;
}

void DeyeInverter::sendSocketMessage(String message) {

    Serial.println("Sending deye message: "+message);

    //IPAddress RecipientIP(192, 168, 1, 138);
    _socket->beginPacket(*_ipAdress, _port);
    _socket->print(message);
    _socket->endPacket();

    _timerTimeoutCheck = millis();
}


void DeyeInverter::updateSocket() {

    if (!WiFi.isConnected()) {
        Serial.println("Wifi not connected");
        _socket = nullptr;
        return;
    }

    if(_ipAdress == nullptr){
        if(_timerResolveHostname == 0 or millis() - _timerResolveHostname > (TIMER_RESOLVE_HOSTNAME)){
            _timerResolveHostname = millis();
            resolveHostname();
        }
    }

    if(_ipAdress == nullptr){
        return;
    }

    if (_timerHealthCheck != 0 and millis() - _timerHealthCheck < (TIMER_HEALTH_CHECK)) {
        //no fetch needed
        return;
    }

    if (_socket == nullptr) {
        if (millis() - _timerErrorBackOff < (TIMER_ERROR_BACKOFF)) {
            //wait after error for try again
            return;
        }
        Serial.println("New connection");
        _socket = std::make_unique<WiFiUDP>();
        Serial.print("port: ");
        sendSocketMessage("WIFIKIT-214028-READ");
        _startCommand = true;
        _timerBetweenSends = 0;
    }

    int packetSize = _socket->parsePacket();
    while (packetSize > 0){
        Serial.println("Recevied new package");
        size_t num = _socket->read(_readBuff,packetSize);
        _socket->flush();
        _timerTimeoutCheck = millis();
        Serial.println(num);
        if(_startCommand){
            if(!parseInitInformation(num)){
                sendSocketMessage("AT+Q\n");
                _socket->stop();
                _socket = nullptr;
                return;
            }
            _startCommand = false;
            sendSocketMessage("+ok");
            //sendCurrentRegisterRead();
            _timerBetweenSends = millis();
        }else{
            int ret = handleRegisterRead(num);
            if(ret == 0){//ok
                if(_commandPosition == INIT_COMMAND_START_SKIP && !_needInitData && (millis() - _timerFullPoll < (TIMER_FETCH_DATA))){
                    sendSocketMessage("AT+Q\n");
                    _socket->stop();
                    _socket = nullptr;
                    _commandPosition = INIT_COMMAND_START_SKIP;
                    _timerHealthCheck = millis();
                    spwapBuffers();
                    Serial.println("Succesfully healtcheck");
                    return;
                }
                if(_commandPosition +1 >= getRegisteresToRead().size()){
                    sendSocketMessage("AT+Q\n");
                    _socket->stop();
                    _socket = nullptr;
                    _commandPosition = INIT_COMMAND_START_SKIP;
                    spwapBuffers();
                    Serial.println("Red succesfull all values");
                    _timerHealthCheck = millis();
                    if(_needInitData){
                        _needInitData = false;
                        _timerFullPoll = millis();
                    }else{
                        //so do exactly match 5 minutes of logger checking data
                        while(millis() - _timerFullPoll > (TIMER_FETCH_DATA)){
                            _timerFullPoll += TIMER_FETCH_DATA;
                        }
                    }
                    return;
                }
                _timerBetweenSends = millis();
                _commandPosition++;
                //pollWait = true;
                //sendCurrentRegisterRead();
            }else{
                _timerErrorBackOff = millis();
                sendSocketMessage("AT+Q\n");
                _socket->stop();
                _socket = nullptr;
                return;
            }
        }
        packetSize = _socket->parsePacket();
    }

    if(_timerBetweenSends != 0){
        if (millis() - _timerBetweenSends > TIMER_BETWEEN_SENDS) {
            sendCurrentRegisterRead();
            _timerBetweenSends = 0;
        }
    }else {
        //timeout of one second
        if (millis() - _timerTimeoutCheck > TIMER_TIMEOUT) {
            Serial.println("Max poll time overtook try again");
            sendSocketMessage("AT+Q\n");
            _socket->stop();
            _socket = nullptr;
        }
    }
}

uint64_t DeyeInverter::serial() const {
    return _serial;
}

String DeyeInverter::typeName() const {
    return _devInfoParser->getHwModelName();
}

bool DeyeInverter::isProducing() {
    return _statisticsParser->getChannelFieldValue(TYPE_AC,CH0,FLD_PAC) > 0;
}

bool DeyeInverter::isReachable() {
    return millis() - _timerHealthCheck < 25 * 1000;
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
    strncpy(_hostnameOrIp, hostOrIp, len);
    _hostnameOrIp[len] = '\0';

}

void DeyeInverter::setPort(uint16_t port) {
    _port = port;
}

bool DeyeInverter::parseInitInformation(size_t length) {
    String ret = String(_readBuff,length);
    Serial.print("Recevied Initial Read: ");
    Serial.println(ret);

    int index = ret.lastIndexOf(',');
    if(index < 0){
        //TODO error logging
        return false;
    }
    if(index >= ret.length()-1){
        //TODO error logging
        return false;
    }

    String serial = ret.substring(index+1);

    if(!serial.equalsIgnoreCase(_serialString)){
        //TODO error parsing
        Serial.write("Serial dose not match");
        return false;
    }

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

    auto & current = getRegisteresToRead()[_commandPosition];

    if(current.readRegister.startsWith("AT+")){
        if(current.readRegister == "AT+YZVER"){
            int start = 4;

            if(ret.length() <= start){
                Serial.print("Error while reading data not entoh data on: ");
                Serial.println(current.readRegister);
                return -2000;
            }

            _devInfoParser->setHardwareVersion(ret.substring(start));
        }
        return 0;
    }

    if(current.length == 2){
        if(!ret.startsWith("+ok=010304")){
            Serial.println("Length for 2 not matching");
            return -1;
        }
    }if(current.length == 5){
        if(!ret.startsWith("+ok=01030A")){
            Serial.println("Length for 5 not matching");
            return -1;
        }
    }else if(current.length == 1){
        if(!ret.startsWith("+ok=010302")){
            Serial.println("Length for 1 not matching");
            return -1;
        }
    }else{
        Serial.println("Unknown Length");
    }

    //+ok= plus first 6 header characters
    int start = 4 + 6;

    if(ret.length() < start+(current.length*2)){
        Serial.print("Error while reading data not entoh data on: ");
        Serial.println(current.readRegister);
        return -1000;
    }

    if(current.length > 4){
        appendFragment(current.targetPos,(uint8_t*)(ret.c_str()),current.length*2);//I know this is string cast is ugly
    }else {

        String hexString = ret.substring(start, start + (current.length * 4));

        if (current.length == 2) {
            Serial.println("Perfomring permutation");
            Serial.println(hexString);
            hexString = hexString.substring(4) + hexString.substring(0, 4);
            Serial.println(hexString);
        }

        String finalResult;

        for (int i = 0; i < hexString.length(); i = i + 2) {
            unsigned number = hex_char_to_int(hexString[i]); // most signifcnt nibble
            unsigned lsn = hex_char_to_int(hexString[i + 1]); // least signt nibble
            number = (number << 4) + lsn;
            finalResult += (char) number;
        }

        if(current.readRegister == "005A"){//temperature needs minus 10
            int16_t value = (int16_t)DeyeUtils::defaultParseUInt(0,(uint8_t*)finalResult.c_str(),1);

            value -= 1000;

            if(value < -1000){
                value = -1000;
            }

            Serial.print("Caluclated value is: ");
            Serial.println(value);

            std::stringstream stream;
            stream << std::setfill ('0') << std::setw(sizeof(int16_t)*2)<< std::hex << value;
            hexString = stream.str().c_str();

            finalResult = "";

            for (int i = 0; i < hexString.length(); i = i + 2) {
                unsigned number = hex_char_to_int(hexString[i]); // most signifcnt nibble
                unsigned lsn = hex_char_to_int(hexString[i + 1]); // least signt nibble
                number = (number << 4) + lsn;
                finalResult += (char) number;
            }
        }

        appendFragment(current.targetPos,(uint8_t*)(finalResult.c_str()),current.length*2);//I know this is string cast is ugly
    }

    return 0;
}

void DeyeInverter::appendFragment(uint8_t offset, uint8_t* payload, uint8_t len)
{
    if (offset + len > STATISTIC_PACKET_SIZE) {
        Serial.printf("FATAL: (%s, %d) stats packet too large for buffer\r\n", __FILE__, __LINE__);
        return;
    }
    memcpy(&_payloadStatisticBuffer[offset], payload, len);
}

void DeyeInverter::sendCurrentRegisterRead() {

    if((_commandPosition == INIT_COMMAND_START_SKIP && !_needInitData ) ||
       (_commandPosition == 0 && _needInitData)){
        //sendSocketMessage("+ok");
    }

    auto & current = getRegisteresToRead()[_commandPosition];

    if(current.readRegister.startsWith("AT+")){
        sendSocketMessage(current.readRegister+"\n");
        return;
    }

    String data = "0103";
    data += current.readRegister;

    std::stringstream ss;
    ss << std::setw(4) << std::setfill('0') << (int)current.length;
    data += ss.str().c_str();

    String checksum = modbusCRC16FromASCII(data);

    sendSocketMessage("AT+INVDATA=8,"+data+checksum+"\n");
}

void DeyeInverter::spwapBuffers() {
    _statisticsParser->beginAppendFragment();
    _statisticsParser->clearBuffer();
    _statisticsParser->appendFragment(0,_payloadStatisticBuffer,STATISTIC_PACKET_SIZE);
    _statisticsParser->setLastUpdate(millis());
    _statisticsParser->resetRxFailureCount();
    _statisticsParser->endAppendFragment();

    _systemConfigParaParser->setLimitPercent(DeyeUtils::defaultParseFloat(42,_payloadStatisticBuffer));

    _devInfoParser->clearBuffer();
    _devInfoParser->appendFragment(0,_payloadStatisticBuffer+44,2);
    _devInfoParser->setLastUpdate(millis());
}

bool DeyeInverter::resolveHostname() {
    DNSClient dns;
    IPAddress remote_addr;

    Serial.print("Try to resolve hostname: ");
    Serial.println(_hostnameOrIp);

    dns.begin(WiFi.dnsIP());
    auto ret = dns.getHostByName(_hostnameOrIp, remote_addr);
    if (ret == 1) {
        Serial.print("Resolved Ip is: ");
        Serial.println(remote_addr);
        _ipAdress = std::make_unique<IPAddress>(remote_addr);
        return true;
    }
    Serial.println("Could not resolve hostname");
    return false;
}

inverter_type DeyeInverter::getInverterType() {
    return inverter_type::Inverter_DeyeSun;
}

String DeyeInverter::serialToModel(uint64_t serial) {

    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             ((uint32_t)((serial >> 32) & 0xFFFFFFFF)),
             ((uint32_t)(serial & 0xFFFFFFFF)));
    String serialString = serial_buff;

    if(serialString.startsWith("415")){//TODO find out more ids and check if correct
        return "SUN600G3-EU-230";
    }else if(serialString.startsWith("413")){//TODO find out more ids and check if correct
        return "SUN300G3-EU-230";
    }

    return "Unknown Deye Sun Inverter";
}

