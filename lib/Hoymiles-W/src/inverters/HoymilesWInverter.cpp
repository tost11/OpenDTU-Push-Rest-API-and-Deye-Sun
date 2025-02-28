#include "HoymilesWInverter.h"
#include "Dns.h"

#include <cstring>
#include <ios>
#include <iomanip>

HoymilesWInverter::HoymilesWInverter(uint64_t serial)
{
    _serial = serial;

    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             ((uint32_t)((serial >> 32) & 0xFFFFFFFF)),
             ((uint32_t)(serial & 0xFFFFFFFF)));
    _serialString = serial_buff;

    _alarmLogParser.reset(new HoymilesWAlarmLog());
    _devInfoParser.reset(new HoymilesWDevInfo());
    _powerCommandParser.reset(new PowerCommandParser());
    _statisticsParser.reset(new HoymilesWStatisticsParser());
    _systemConfigParaParser.reset(new SystemConfigParaParser());

    _devInfoParser->setMaxPowerDevider(10);

    _dataUpdateTimer.set(31 * 1000);
    _dataUpdateTimer.zero();
    _dataStatisticTimer.set(5 * 60 * 1000);
    _dataStatisticTimer.zero();

    _clearBufferOnDisconnect = false;

    _enablePolling = true;//TODO make better
}

void HoymilesWInverter::update() {

    if(_IpOrHostnameIsMac){
        checkForMacResolution();
    }

    if(_resolvedIpByMacAdress != nullptr && *_resolvedIpByMacAdress != std::string(_dtuInterface.getServer().c_str())){
        MessageOutput.println("Set new ip by mac to DTU interface");
        _dtuInterface.setServerAndPort(String(_resolvedIpByMacAdress->c_str()),_port);
    }

    getEventLog()->checkErrorsForTimeout();

    if(_dtuInterface.isConnected()){
        if(_dataStatisticTimer.occured()){
            if(_dtuInterface.requestStatisticUpdate()){
                _dataStatisticTimer.reset();
            }
        }
        if(_dataUpdateTimer.occured()){
            if(_dtuInterface.requestDataUpdate()){
                _dataUpdateTimer.reset();
            }
        }
        _clearBufferOnDisconnect = false;
    }else{
        if(!_clearBufferOnDisconnect){//TODO make better
            _clearBufferOnDisconnect = true;
            _statisticsParser->beginAppendFragment();
            _statisticsParser->clearBuffer();
            _statisticsParser->incrementRxFailureCount();
            _statisticsParser->endAppendFragment();
        }
        _dataUpdateTimer.zero();
        _dataStatisticTimer.zero();
    }
    
    auto data = _dtuInterface.newDataAvailable();
    //TODO refactor
    if(data != nullptr){
        _dtuInterface.printDataAsTextToSerial();
        if(_dtuInterface.isSerialValid(_serial)){
            swapBuffers(data.get());
        }else{
            MessageOutput.printf("Skipped setting red inverter data because serial isn valid! (dtu red %llu to configured: %llu)\n",_dtuInterface.getRedSerial(),_serial);
            _alarmLogParser->addAlarm(6,60,"received inverter data not used because serials not matching (correct serial and ip?)");

            //anyway set timer so its shown as online but not production (helps for debugging error)
            _devInfoParser->setLastUpdate(millis());
        }
    }
}

void HoymilesWInverter::swapBuffers(const InverterData *data) {
    _statisticsParser->beginAppendFragment();
    _statisticsParser->clearBuffer();
    _statisticsParser->appendFragment(0, (const uint8_t*)data, sizeof(InverterData));
    _statisticsParser->setLastUpdate(millis());
    _statisticsParser->resetRxFailureCount();
    _statisticsParser->endAppendFragment();

    _systemConfigParaParser->setLimitPercent(data->powerLimit);

    _devInfoParser->clearBuffer();
    //TODO implement
    //_devInfoParser->appendFragment(0,_payloadStatisticBuffer+44,2);
    _devInfoParser->setLastUpdate(millis());
}

uint64_t HoymilesWInverter::serial() const {
    return _serial;
}

String HoymilesWInverter::typeName() const {
    return _devInfoParser->getHwModelName();
}

bool HoymilesWInverter::isProducing() {
    auto stats = getStatistics();
    float totalAc = 0;
    for (auto& c : stats->getChannelsByType(TYPE_AC)) {
        if (stats->hasChannelFieldValue(TYPE_AC, c, FLD_PAC)) {
            totalAc += stats->getChannelFieldValue(TYPE_AC, c, FLD_PAC);
        }
    }

    return _enablePolling && totalAc > 0;
}

bool HoymilesWInverter::isReachable() {
    return _dtuInterface.isConnected();
}

bool HoymilesWInverter::sendActivePowerControlRequest(float limit, PowerLimitControlType type) {
     uint16_t realLimit;
    if(type == RelativPersistent){
        realLimit = (uint16_t)(limit + 0.5);
    }else{
        uint16_t maxPower = _devInfoParser->getMaxPower();
        if(maxPower == 0){
            _alarmLogParser->addAlarm(6,10 * 60,"command not send (max Power) because no known limit for this inverter is available (serial uknown)");//alarm for 10 min
            getSystemConfigParaParser()->setLastLimitRequestSuccess(CMD_NOK);
            return false;
        }
        realLimit = (uint16_t)(limit / (float)maxPower * 100);
    }
    if(realLimit > 100){
        realLimit = 100;
    }
    if(!_dtuInterface.isSerialValid(_serial)){
        _alarmLogParser->addAlarm(6,10 * 60,"command not send (max Power) because serials not matching (correct serial and ip?)");//alarm for 10 min
        return false;
    }
    _dtuInterface.setPowerLimit(realLimit);
    return true;
}

bool HoymilesWInverter::resendPowerControlRequest() {
    return false;//TODO implement
}

bool HoymilesWInverter::sendRestartControlRequest() {
    if(!_dtuInterface.isSerialValid(_serial)){
        _alarmLogParser->addAlarm(6,10 * 60,"command not send (restart) because serials not matching (correct serial and ip?)");//alarm for 10 min
        return false;
    }
    _dtuInterface.requestRestartDevice();
    return true;
}

bool HoymilesWInverter::sendPowerControlRequest(bool turnOn) {
    return false;//TODO implement
}

inverter_type HoymilesWInverter::getInverterType() const {
    return inverter_type::Inverter_HoymilesW;
}

void HoymilesWInverter::setEnableCommands(const bool enabled) {
    BaseInverter::setEnableCommands(enabled);
}

void HoymilesWInverter::hostOrPortUpdated(){
    String ip = _oringalIpOrHostname;
    if(_resolvedIpByMacAdress != nullptr){
        ip = _resolvedIpByMacAdress->c_str();
    }
    if(ip != _dtuInterface.getServer() || _port != _dtuInterface.getPort()){
        MessageOutput.println("Set new ip to DTU interface");
        _dtuInterface.setServerAndPort(ip,_port);
    }
}

void HoymilesWInverter::startConnection(){
    _dtuInterface.setup();
}

bool HoymilesWInverter::supportsPowerDistributionLogic()
{
    return false;
}