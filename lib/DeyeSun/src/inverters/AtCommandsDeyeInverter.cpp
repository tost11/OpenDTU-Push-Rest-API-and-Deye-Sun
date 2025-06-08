#include "AtCommandsDeyeInverter.h"

#include "DeyeUtils.h"
#include "DeyeSun.h"

#include <Dns.h>
#include <MessageOutput.h>


AtCommandsDeyeInverter::AtCommandsDeyeInverter(uint64_t serial):
DeyeInverter(serial),
_socket(nullptr){

    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             ((uint32_t)((serial >> 32) & 0xFFFFFFFF)),
             ((uint32_t)(serial & 0xFFFFFFFF)));
    _serialString = serial_buff;

    _devInfoParser->setMaxPowerDevider(10);

    _needInitData = true;
    _commandPosition = 0;
    _errorCounter = -1;

    _ipAdress = nullptr;
}

void AtCommandsDeyeInverter::sendSocketMessage(String message) {

    MessageOutput.printlnDebug("Deye AT-Commands -> Sending deye message: "+message);

    _socket->beginPacket(*_ipAdress, _port);
    _socket->print(message);
    _socket->endPacket();

    _timerTimeoutCheck.set(TIMER_TIMEOUT);
}


void AtCommandsDeyeInverter::update() {

    getEventLog()->checkErrorsForTimeout();

    if (_ipAdress == nullptr) {
        if (_timerResolveHostname.occured()) {
            if(resolveHostname()){
                _timerResolveHostname.set(TIMER_RESOLVE_HOSTNAME_LONG);
            }else{
                _timerResolveHostname.set(TIMER_RESOLVE_HOSTNAME);
            }
        }
    }else{
        if (_timerResolveHostname.occured()) {
            if(resolveHostname()){
                _timerResolveHostname.set(TIMER_RESOLVE_HOSTNAME_LONG);
            }else{
                MessageOutput.println("Deye AT-Commands -> Resolved hostname isn't valid anymore -> reset resolved ip");
                _ipAdress = nullptr;
                _timerResolveHostname.set(TIMER_RESOLVE_HOSTNAME);
            }
        }
    }

    if (_ipAdress == nullptr) {
        return;
    }

    //polling is disabled (night whatever) wait for existing socket connection and command if null not active skip check
    if(_socket == nullptr && !getEnablePolling()){
        return;
    }

    if(!_timerAfterCounterTimout.occured()) {
        //wait after error for try again
        return;
    }

    if(_currentWritCommand == nullptr){
        bool busy = handleRead();
        if(!busy && _currentWritCommand == nullptr && getEnableCommands()) {
            if (_powerTargetStatus != nullptr) {
                MessageOutput.printlnDebug("Deye AT-Commands -> Start writing register power status");
                _currentWritCommand = std::make_unique<WriteRegisterMapping>("002B", 1,*_powerTargetStatus ? "0001" : "0002");
                _powerTargetStatus = nullptr;
            } else if (_limitToSet != nullptr) {
                MessageOutput.printlnDebug("Deye AT-Commands -> Start writing register limit");
                _currentWritCommand = std::make_unique<WriteRegisterMapping>("0028", 1, String(DeyeUtils::lengthToHexString(*_limitToSet,4).c_str()));
                _limitToSet = nullptr;
            }
        }
    }

    if(_currentWritCommand != nullptr){
        handleWrite();
    }
}

bool AtCommandsDeyeInverter::isReachable() {
    return _timerHealthCheck.dist() < 60 * 1000;
}

bool AtCommandsDeyeInverter::sendActivePowerControlRequest(float limit, PowerLimitControlType type) {
    //TODO do better
    if(typeName().startsWith("Unknown") && !DeyeSun.getUnknownDevicesWriteEnable()){
        _alarmLogParser->addAlarm(6,10 * 60,"limit command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");//alarm for 10 min
        MessageOutput.println("Deye AT-Commands -> limit command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");
        return false;
    }
    if(!(type == AbsolutPersistent || type == RelativPersistent)){
        MessageOutput.println("Deye AT-Commands -> Setting of temporary limit on deye inverter not possible");
        return false;
    }

    uint16_t realLimit;
    if(type == RelativPersistent){
        realLimit = (uint16_t)(limit + 0.5f);
    }else{
        uint16_t maxPower = _devInfoParser->getMaxPower();
        if(maxPower == 0){
            _alarmLogParser->addAlarm(6,10 * 60,"limit command not send because init data of device not received yet (max Power)");//alarm for 10 min
            getSystemConfigParaParser()->setLastLimitRequestSuccess(CMD_NOK);
            return false;
        }
        realLimit = (uint16_t)(limit / (float)maxPower * 100);
    }
    if(realLimit > 100){
        realLimit = 100;
    }
    getSystemConfigParaParser()->setLastLimitRequestSuccess(CMD_PENDING);
    _limitToSet = std::make_unique<uint16_t>(realLimit);
    return true;
}

bool AtCommandsDeyeInverter::resendPowerControlRequest() {
    return false;
}

bool AtCommandsDeyeInverter::sendRestartControlRequest() {
    return false;
}

bool AtCommandsDeyeInverter::sendPowerControlRequest(bool turnOn) {
    if(typeName().startsWith("Unknown") && !DeyeSun.getUnknownDevicesWriteEnable()){
        _alarmLogParser->addAlarm(6,10 * 60,"power command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");//alarm for 10 min
        MessageOutput.println("Deye AT-Commands -> power command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");
        return false;
    }
    _powerTargetStatus = std::make_unique<bool>(turnOn);
    _powerCommandParser->setLastPowerCommandSuccess(CMD_PENDING);
    return true;
}

bool AtCommandsDeyeInverter::parseInitInformation(size_t length) {
    String ret = String(_readBuff,length);
    MessageOutput.printlnDebug("Deye AT-Commands -> Received Initial Read: " + ret);

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
        _alarmLogParser->addAlarm(5,10 * 60);//alarm for 10 min
        MessageOutput.println("Deye AT-Commands -> Serial dose not match");
        return false;
    }

    return true;
}

String AtCommandsDeyeInverter::filterReceivedResponse(size_t length){
    String ret;
    for(int i=0;i<length;i++){
        //there are those od characters in response filter them
        if(_readBuff[i] >= 32){
            ret+=_readBuff[i];
        }
    }
    return ret;
}

int AtCommandsDeyeInverter::handleRegisterWrite(size_t length) {
    String ret = filterReceivedResponse(length);

    MessageOutput.printlnDebug("Deye AT-Commands -> Filtered received register write: " + ret);

    if(ret.startsWith("+ERR=")) {
        if (ret.startsWith("+ERR=-1")) {
            return -1;
        }
        return -2;
    }

    String expected = "+ok=0110";
    expected += _currentWritCommand->writeRegister;
    expected += "0001";

    //todo checksum
    if(!ret.startsWith(expected)) {
        MessageOutput.printlnDebug("Deye AT-Commands -> Write response not correct");
        return -1000;
    }
    return 0;
}

int AtCommandsDeyeInverter::handleRegisterRead(size_t length) {
    String ret= filterReceivedResponse(length);

    MessageOutput.printlnDebug("Deye AT-Commands -> Filtered received Register Read: " + ret);

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
                MessageOutput.printlnDebug("Deye AT-Commands -> Error while reading data not enough data on: " + current.readRegister);
                return -2000;
            }

            _devInfoParser->setHardwareVersion(ret.substring(start));
        }
        return 0;
    }

    //todo add checksum calculation

    if(current.length == 2){
        if(!ret.startsWith("+ok=010304")){
            MessageOutput.printlnDebug("Deye AT-Commands -> Length for 2 not matching");
            return -1;
        }
    }if(current.length == 5){
        if(!ret.startsWith("+ok=01030A")){
            MessageOutput.printlnDebug("Deye AT-Commands -> Length for 5 not matching");
            return -1;
        }
    }else if(current.length == 1){
        if(!ret.startsWith("+ok=010302")){
            MessageOutput.printlnDebug("Deye AT-Commands -> Length for 1 not matching");
            return -1;
        }
    }else{
        MessageOutput.printlnDebug("Deye AT-Commands -> Unknown Length");
    }

    //+ok= plus first 6 header characters
    int start = 4 + 6;

    if(ret.length() < start+(current.length*2)){
        MessageOutput.printlnDebug("Deye AT-Commands -> Error while reading data not enough data on: " + current.readRegister);
        return -1000;
    }

    if(current.length > 4){
        appendFragment(current.targetPos,(uint8_t*)(ret.c_str()),current.length*2);//I know this is string cast is ugly
    }else {

        String hexString = ret.substring(start, start + (current.length * 4));

        if (current.length == 2) {
            hexString = hexString.substring(4) + hexString.substring(0, 4);
        }

        String finalResult;

        for (int i = 0; i < hexString.length(); i = i + 2) {
            unsigned number = DeyeUtils::hex_char_to_int(hexString[i]); // most signifcnt nibble
            unsigned lsn = DeyeUtils::hex_char_to_int(hexString[i + 1]); // least signt nibble
            number = (number << 4) + lsn;
            finalResult += (char) number;
        }

        if(current.readRegister == "005A"){//temperature needs minus 10
            int16_t value = (int16_t)DeyeUtils::defaultParseUInt(0,(uint8_t*)finalResult.c_str(),1);

            value -= 1000;

            if(value < -1000){
                value = -1000;
            }

            hexString = String(DeyeUtils::lengthToHexString(value,4).c_str());

            finalResult = "";

            for (int i = 0; i < hexString.length(); i = i + 2) {
                unsigned number = DeyeUtils::hex_char_to_int(hexString[i]); // most signifcnt nibble
                unsigned lsn = DeyeUtils::hex_char_to_int(hexString[i + 1]); // least signt nibble
                number = (number << 4) + lsn;
                finalResult += (char) number;
            }
        }

        appendFragment(current.targetPos,(uint8_t*)(finalResult.c_str()),current.length*2);//I know this is string cast is ugly
    }

    return 0;
}

void AtCommandsDeyeInverter::appendFragment(uint8_t offset, uint8_t* payload, uint8_t len)
{
    if (offset + len > STATISTIC_PACKET_SIZE) {
        MessageOutput.printf("Deye AT-Commands -> FATAL: (%s, %d) stats packet too large for buffer\r\n", __FILE__, __LINE__);
        return;
    }
    memcpy(&_payloadStatisticBuffer[offset], payload, len);
}

void AtCommandsDeyeInverter::sendCurrentRegisterRead() {
    auto & current = getRegisteresToRead()[_commandPosition];

    if(current.readRegister.startsWith("AT+")){
        sendSocketMessage(current.readRegister+"\n");
        return;
    }

    String data = "0103";
    data += current.readRegister;

    data += DeyeUtils::lengthToString(current.length,4);

    String checksum = DeyeUtils::modbusCRC16FromASCII(data);

    sendSocketMessage("AT+INVDATA=8,"+data+checksum+"\n");

    ConnectionStatistics.SendCommands++;
}

void AtCommandsDeyeInverter::sendCurrentRegisterWrite() {

    if(_currentWritCommand->length * 2 * 2 != _currentWritCommand->valueToWrite.length()){
        MessageOutput.printf("Deye AT-Commands -> Write register message not correct length, expected: %d is: %d\n",_currentWritCommand->length * 2 * 2,_currentWritCommand->valueToWrite.length());
        assert( 0 );
    }

    String data = "0110";
    data += _currentWritCommand->writeRegister;
    data += "0001";
    data += DeyeUtils::lengthToString(_currentWritCommand->length * 2,2);
    data += _currentWritCommand->valueToWrite;

    String checksum = DeyeUtils::modbusCRC16FromASCII(data);

    MessageOutput.printlnDebug("Deye AT-Commands -> Sending socket message write: ");
    //Serial.println("AT+INVDATA=11,"+data+checksum+"\n");
    sendSocketMessage("AT+INVDATA=11,"+data+checksum+"\n");

    ConnectionStatistics.SendCommands++;
}

void AtCommandsDeyeInverter::swapBuffers(bool fullData) {
    if(fullData) {

        _statisticsParser->beginAppendFragment();
        _statisticsParser->clearBuffer();
        _statisticsParser->appendFragment(0, _payloadStatisticBuffer, STATISTIC_PACKET_SIZE);
        _statisticsParser->setLastUpdate(millis());
        _statisticsParser->resetRxFailureCount();
        _statisticsParser->endAppendFragment();

        _devInfoParser->clearBuffer();
        _devInfoParser->appendFragment(0,_payloadStatisticBuffer+44,2);

        //set new deye offline offset if configured
        for (auto& type : _statisticsParser->getChannelTypes()) {
            for (auto &channel: _statisticsParser->getChannelsByType(type)) {
                if(!_statisticsParser->hasChannelFieldValue(type,channel,FLD_YD)){
                    continue;
                }
                _statisticsParser->setChannelFieldOffset(type, channel, FLD_YD, 0, 2);
                if(_statisticsParser->getDeyeSunOfflineYieldDayCorrection()) {
                    float currentOffset = _statisticsParser->getChannelFieldOffset(type, channel, FLD_YD,1);
                    MessageOutput.printfDebug("Deye AT-Commands -> Current Deye Offline offset: %f\n",currentOffset);
                    if (!(currentOffset > 0.f || currentOffset < 0.f)) {
                        float val = _statisticsParser->getChannelFieldValue(type, channel, FLD_YD);
                        _statisticsParser->setChannelFieldOffset(type, channel, FLD_YD, val * -1.f,1);
                        float checkOffset = _statisticsParser->getChannelFieldOffset(type, channel, FLD_YD,1);
                        MessageOutput.printf("Deye AT-Commands -> Set daily production offset for type: %d and channel: %d to:%f\n",(int)type,(int)channel,checkOffset);
                    }
                }else{
                    //MessageOutput.println("nope");
                    if(_statisticsParser->getSettingByChannelField(type,channel,FLD_YD,1) != nullptr){
                        _statisticsParser->setChannelFieldOffset(type, channel, FLD_YD, 0, 1);
                    }
                }
            }
        }

        _devInfoParser->setLastUpdate(millis());
    }

    _systemConfigParaParser->setLimitPercent(DeyeUtils::defaultParseFloat(42,_payloadStatisticBuffer));
}

bool AtCommandsDeyeInverter::resolveHostname() {

    if(_IpOrHostnameIsMac){
        checkForMacResolution();
    }

    DNSClient dns;
    IPAddress remote_addr;

    const char * ipToFind = (_resolvedIpByMacAdress != nullptr) ? _resolvedIpByMacAdress->c_str() : _oringalIpOrHostname.c_str();

    MessageOutput.printlnDebug(String("Deye AT-Commands -> Try to resolve hostname: ") + ipToFind);

    dns.begin(WiFi.dnsIP());
    auto ret = dns.getHostByName(ipToFind, remote_addr);
    if (ret == 1) {
        MessageOutput.printlnDebug("Deye AT-Commands -> Resolved Ip is: " + remote_addr.toString());
        _ipAdress = std::make_unique<IPAddress>(remote_addr);
        return true;
    }
    MessageOutput.printf("Deye AT-Commands -> Could not resolve hostname: %s\n", ipToFind);
    return false;
}

bool AtCommandsDeyeInverter::handleRead() {
    if (!_timerHealthCheck.occured()) {
        //no fetch needed
        return false;
    }

    if (_socket == nullptr) {
        _errorCounter++;
        if(_errorCounter > 50){//give up after some failed attempts and wait long
            _commandPosition = _needInitData ? 0 : INIT_COMMAND_START_SKIP;
            _timerAfterCounterTimout.set(TIMER_COUNTER_ERROR_TIMEOUT);
            _errorCounter = -1;
            MessageOutput.printf("Deye AT-Commands -> Read Data of Timeout (or not reachable) of Deye Sun Inverter: %s\n", name());
            return true;//busy true so timeout works and so write is send
        }
        if (!_timerErrorBackOff.occured()) {
            //wait after error for try again
            return true;
        }
        MessageOutput.printfDebug("Deye AT-Commands -> New connection to: %s\n",(_ipAdress != nullptr ? _ipAdress->toString().c_str():""));
        _socket = std::make_unique<WiFiUDP>();
        sendSocketMessage("WIFIKIT-214028-READ");
        _startCommand = true;
        _timerBetweenSends = 0;
        ConnectionStatistics.Connects++;
    }

    int packetSize = _socket->parsePacket();
    while (packetSize > 0){
        MessageOutput.printlnDebug("Deye AT-Commands -> Received new package");
        size_t num = _socket->read(_readBuff,packetSize);
        _socket->flush();
        _timerTimeoutCheck.set(TIMER_TIMEOUT);
        if(_startCommand){
            if(!parseInitInformation(num)){
                endSocket();
                _errorCounter = 1000;
                return true;
            }
            _startCommand = false;
            sendSocketMessage("+ok");
            _timerBetweenSends = millis();
            ConnectionStatistics.ConnectsSuccessful++;
            ConnectionStatistics.HealthChecks++;
        }else{
            int ret = handleRegisterRead(num);
            if(ret == 0){//ok
                if(_commandPosition == LAST_HEALTHCHECK_COMMEND){
                    if(!_needInitData && !_timerFullPoll.occured()) {
                        endSocket();
                        _commandPosition = INIT_COMMAND_START_SKIP;
                        _timerHealthCheck.set(getInternalPollTime());
                        swapBuffers(false);
                        _errorCounter = -1;
                        MessageOutput.println("Deye AT-Commands -> Successfully healthcheck");
                        ConnectionStatistics.HealthChecksSuccessful++;
                        return false;
                    }else{
                        ConnectionStatistics.ReadRequests++;
                        ConnectionStatistics.HealthChecksSuccessful++;//not fully correct but looks good on statistics TODO fix to correct behavior
                    }
                }
                if(_commandPosition + 1 >= getRegisteresToRead().size()){
                    endSocket();
                    _commandPosition = INIT_COMMAND_START_SKIP;
                    swapBuffers(true);
                    _timerHealthCheck.set(getInternalPollTime());
                    _errorCounter = -1;
                    if(_needInitData){
                        _needInitData = false;
                        _timerFullPoll.set(TIMER_FULL_POLL);
                    }else{
                        //so do exactly match 5 minutes of logger checking data
                        while(_timerFullPoll.occured()){
                            _timerFullPoll.extend(TIMER_FULL_POLL);
                        }
                    }
                    MessageOutput.println("Deye AT-Commands -> Red successful all values");
                    ConnectionStatistics.ReadRequestsSuccessful++;
                    return false;
                }
                _timerBetweenSends = millis();
                _commandPosition++;
                //pollWait = true;
                //sendCurrentRegisterRead();
            }else{
                _timerErrorBackOff.set(TIMER_ERROR_BACKOFF);
                endSocket();
                ConnectionStatistics.ErrorCommands++;
                return true;
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
        //timeout of one ca. second
        if (_timerTimeoutCheck.occured()) {
            MessageOutput.printlnDebug("Deye AT-Commands -> Max poll time overtook try again");
            endSocket();

            if(!_startCommand){
                ConnectionStatistics.TimeoutCommands++;
            }
        }
    }
    return true;
}

void AtCommandsDeyeInverter::endSocket() {
    if(_socket != nullptr) {
        sendSocketMessage("AT+Q\n");
        _socket->stop();
        _oldSocket = std::move(_socket);
    }
}

void AtCommandsDeyeInverter::handleWrite() {
    if (_socket == nullptr) {
        _errorCounter++;
        if(_errorCounter > 10){
            _errorCounter = -1;
            //TODO do better
            if(_currentWritCommand->writeRegister == "002B") {
                _powerCommandParser->setLastPowerCommandSuccess(CMD_NOK);
                _alarmLogParser->addAlarm(7,10 * 60);//alarm for 10 min
            }else if(_currentWritCommand->writeRegister == "0028") {
                _systemConfigParaParser->setLastLimitRequestSuccess(CMD_NOK);
                _alarmLogParser->addAlarm(6,10 * 60);//alarm for 10 min
            }
            _currentWritCommand = nullptr;
            return;
        }
        if (!_timerErrorBackOff.occured()) {
            //wait after error for try again
            return;
        }
        MessageOutput.printlnDebug("Deye AT-Commands -> New connection for write");
        _socket = std::make_unique<WiFiUDP>();
        sendSocketMessage("WIFIKIT-214028-READ");
        _startCommand = true;
        _timerBetweenSends = 0;
        ConnectionStatistics.Connects++;
    }

    int packetSize = _socket->parsePacket();
    while (packetSize > 0){
        MessageOutput.printlnDebug("Deye AT-Commands -> Received new package for write");
        size_t num = _socket->read(_readBuff,packetSize);
        _socket->flush();
        _timerTimeoutCheck.set(TIMER_TIMEOUT);
        if(_startCommand){
            if(!parseInitInformation(num)){
                _errorCounter = 1000;
                endSocket();
            }
            ConnectionStatistics.ConnectsSuccessful++;
            ConnectionStatistics.WriteRequests++;
            _startCommand = false;
            sendSocketMessage("+ok");
            //sendCurrentRegisterRead();
            _timerBetweenSends = millis();
        }else{
            int ret = handleRegisterWrite(num);
            if(ret == 0){//ok
                endSocket();
                //TODO do better
                if(_currentWritCommand->writeRegister == "002B") {
                    _powerCommandParser->setLastPowerCommandSuccess(CMD_OK);
                }else if(_currentWritCommand->writeRegister == "0028") {
                    _systemConfigParaParser->setLastLimitRequestSuccess(CMD_OK);
                }
                _currentWritCommand = nullptr;
                ConnectionStatistics.WriteRequestsSuccessful++;
                return;
            }
            _timerErrorBackOff.set(TIMER_ERROR_BACKOFF);
            endSocket();
            ConnectionStatistics.ErrorCommands++;
            return;
        }
        packetSize = _socket->parsePacket();
    }

    if(_timerBetweenSends != 0){
        if (millis() - _timerBetweenSends > TIMER_BETWEEN_SENDS) {
            sendCurrentRegisterWrite();
            _timerBetweenSends = 0;
        }
    }else {
        //timeout of one second
        if (_timerTimeoutCheck.occured()) {
            MessageOutput.printlnDebug("Deye AT-Commands -> Max poll time for write overtook from write, try again");
            endSocket();
            if(!_startCommand){
                ConnectionStatistics.TimeoutCommands++;
            }
        }
    }
}

void AtCommandsDeyeInverter::setEnableCommands(const bool enabled) {
    BaseInverter::setEnableCommands(enabled);
    //remove not yet handles set commands
    _limitToSet = nullptr;
    _powerTargetStatus = nullptr;
}

bool AtCommandsDeyeInverter::supportsPowerDistributionLogic()
{
    return false;
}

uint64_t AtCommandsDeyeInverter::getInternalPollTime() {
    uint64_t time = _pollTime;
    if(time < 5){//just to be sure
        time = 5;
    }
    time = time * 1000;
    return time;
}

void AtCommandsDeyeInverter::resetStats() {
    ConnectionStatistics = {};
}

void AtCommandsDeyeInverter::onPollTimeChanged() {
    _timerHealthCheck.setTimeout(getInternalPollTime());
}

deye_inverter_type AtCommandsDeyeInverter::getDeyeInverterType() const {
    return Deye_Sun_At_Commands;
}
