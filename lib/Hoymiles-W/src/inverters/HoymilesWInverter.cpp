#include "HoymilesWInverter.h"
#include "Dns.h"

#include <cstring>
#include <sstream>
#include <ios>
#include <iomanip>

HoymilesWInverter::HoymilesWInverter(uint64_t serial,Print & print):
_messageOutput(print)
{
    _serial = serial;

    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             ((uint32_t)((serial >> 32) & 0xFFFFFFFF)),
             ((uint32_t)(serial & 0xFFFFFFFF)));
    _serialString = serial_buff;

    _alarmLogParser.reset(new HoymilesWAlarmLog());
    _devInfoParser.reset(new HoymilesWDevInfo());
    _gridProfileParser.reset(new HoymilesWGridProfile());
    _powerCommandParser.reset(new PowerCommandParser());
    _statisticsParser.reset(new HoymilesWStatisticsParser());
    _systemConfigParaParser.reset(new SystemConfigParaParser());

    _devInfoParser->setMaxPowerDevider(10);

    _dataUpdateTimer.set(30 * 1000);
    _dataUpdateTimer.zero();
    _dataStatisticTimer.set(5 * 60 * 1000);
    _dataStatisticTimer.zero();

    _clearBufferOnDisconnect = false;

    _enablePolling = true;//TODO make better
}

void HoymilesWInverter::update() {

    EventLog()->checkErrorsForTimeout();

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
    if(data.get() != nullptr){
        _dtuInterface.printDataAsTextToSerial();
        swapBuffers(data.get());
    }

}

void HoymilesWInverter::swapBuffers(const InverterData *data) {
    _statisticsParser->beginAppendFragment();
    _statisticsParser->clearBuffer();
    _statisticsParser->appendFragment(0, (const uint8_t*)data, sizeof(InverterData));
    _statisticsParser->setLastUpdate(millis());
    _statisticsParser->resetRxFailureCount();
    _statisticsParser->endAppendFragment();

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
    auto stats = Statistics();
    float totalAc = 0;
    for (auto& c : stats->getChannelsByType(TYPE_AC)) {
        if (stats->hasChannelFieldValue(TYPE_AC, c, FLD_PAC)) {
            totalAc += stats->getChannelFieldValue(TYPE_AC, c, FLD_PAC);
        }
    }

    return _enablePolling && totalAc > 0;
}

bool HoymilesWInverter::isReachable() {
    return _dtuInterface.isConnected() && (millis() - _devInfoParser->getLastUpdate()) < 120000;//TOTO find better way to check timout
}

bool HoymilesWInverter::sendActivePowerControlRequest(float limit, PowerLimitControlType type) {
    //TODO implement
    return true;
}

bool HoymilesWInverter::resendPowerControlRequest() {
    return false;
}

bool HoymilesWInverter::sendRestartControlRequest() {
    return false;
}

bool HoymilesWInverter::sendPowerControlRequest(bool turnOn) {
    return false;
}

inverter_type HoymilesWInverter::getInverterType() const {
    return inverter_type::Inverter_HoymilesW;
}

void HoymilesWInverter::setEnableCommands(const bool enabled) {
    BaseInverter::setEnableCommands(enabled);
}

void HoymilesWInverter::setHostnameOrIp(const char * hostOrIp){
    _dtuInterface.setServer(hostOrIp);
}

void HoymilesWInverter::setPort(uint16_t port){
    _dtuInterface.setPort(port);
}

void HoymilesWInverter::startConnection(){
        _dtuInterface.setup();
}