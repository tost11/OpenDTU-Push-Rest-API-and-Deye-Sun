//
// Created by lukas on 31.05.25.
//

#include "DeyeInverter.h"
#include "DeyeSun.h"
#include "DeyeUtils.h"

DeyeInverter::DeyeInverter(uint64_t serial){
    _serial = serial;
    _alarmLogParser.reset(new DeyeAlarmLog());
    _devInfoParser.reset(new DeyeDevInfo());
    _powerCommandParser.reset(new PowerCommandParser());
    _statisticsParser.reset(new DefaultStatisticsParser());

    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             static_cast<uint32_t>((serial >> 32) & 0xFFFFFFFF),
             static_cast<uint32_t>(serial & 0xFFFFFFFF));
    _serialString = serial_buff;
}

String DeyeInverter::serialToModel(uint64_t serial) {
    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             ((uint32_t)((serial >> 32) & 0xFFFFFFFF)),
             ((uint32_t)(serial & 0xFFFFFFFF)));
    String serialString = serial_buff;

    if(serialString.startsWith("415") || serialString.startsWith("414") || serialString.startsWith("221")){//TODO find out more ids and check if correct
        return "SUN600G3-EU-230";
    }else if(serialString.startsWith("413") || serialString.startsWith("411")){//TODO find out more ids and check if correct
        return "SUN300G3-EU-230";
    }else if(serialString.startsWith("384") || serialString.startsWith("385") || serialString.startsWith("386")){
        return "SUN-M60/80/100G4-EU-Q0";
    }/*else if(serialString.startsWith("41")){//TODO find out full serial
        return "EPP-300G3-EU-230";
    }*/

    return "Unknown";
}

uint64_t DeyeInverter::serial() const {
    return _serial;
}

bool DeyeInverter::isProducing() {
    auto stats = getStatistics();
    float totalAc = 0;
    for (auto& c : stats->getChannelsByType(TYPE_AC)) {
        if (stats->hasChannelFieldValue(TYPE_AC, c, FLD_PAC)) {
            totalAc += stats->getChannelFieldValue(TYPE_AC, c, FLD_PAC);
        }
    }

    return _enablePolling && totalAc > 0;
}

String DeyeInverter::typeName() const {
    return _devInfoParser->getHwModelName();
}

inverter_type DeyeInverter::getInverterType() const {
    return inverter_type::Inverter_DeyeSun;
}

void DeyeInverter::handleDeyeDayCorrection() {
    //set new deye offline offset if configured
    for (auto& type : _statisticsParser->getChannelTypes()) {
        for (auto &channel: _statisticsParser->getChannelsByType(type)) {
            if(!_statisticsParser->hasChannelFieldValue(type,channel,FLD_YD)){
                continue;
            }
            _statisticsParser->setChannelFieldOffset(type, channel, FLD_YD, 0, 2);
            if(_statisticsParser->getDeyeSunOfflineYieldDayCorrection()) {
                float currentOffset = _statisticsParser->getChannelFieldOffset(type, channel, FLD_YD,1);
                ESP_LOGD(LogTag().c_str(),"Current Deye Offline offset: %f",currentOffset);
                if (!(currentOffset > 0.f || currentOffset < 0.f)) {
                    float val = _statisticsParser->getChannelFieldValue(type, channel, FLD_YD);
                    _statisticsParser->setChannelFieldOffset(type, channel, FLD_YD, val * -1.f,1);
                    float checkOffset = _statisticsParser->getChannelFieldOffset(type, channel, FLD_YD,1);
                    ESP_LOGD(LogTag().c_str(),"Set daily production offset for type: %d and channel: %d to:%f\n",(int)type,(int)channel,checkOffset);
                }
            }else{
                //MessageOutput.println("nope");
                if(_statisticsParser->getSettingByChannelField(type,channel,FLD_YD,1) != nullptr){
                    _statisticsParser->setChannelFieldOffset(type, channel, FLD_YD, 0, 1);
                }
            }
        }
    }
}

bool DeyeInverter::sendActivePowerControlRequest(float limit, PowerLimitControlType type) {
    if(typeName().startsWith("Unknown") && !DeyeSun.getUnknownDevicesWriteEnable()){
        _alarmLogParser->addAlarm(6,10 * 60,"limit command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");//alarm for 10 min
        ESP_LOGI(LogTag().c_str(),"limit command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");
        return false;
    }
    if(!(type == AbsolutPersistent || type == RelativPersistent)){
        ESP_LOGI(LogTag().c_str(),"Setting of temporary limit on deye inverter not possible");
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

bool DeyeInverter::sendPowerControlRequest(bool turnOn) {
    if(typeName().startsWith("Unknown") && !DeyeSun.getUnknownDevicesWriteEnable()){
        _alarmLogParser->addAlarm(6,10 * 60,"power command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");//alarm for 10 min
        ESP_LOGI(LogTag().c_str(),"power command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");
        return false;
    }
    _powerTargetStatus = std::make_unique<bool>(turnOn);
    _powerCommandParser->setLastPowerCommandSuccess(CMD_PENDING);
    return true;
}


bool DeyeInverter::resendPowerControlRequest() {
    //todo implement
    return false;
}

bool DeyeInverter::sendRestartControlRequest() {
    //todo implement
    return false;
}

void DeyeInverter::checkForNewWriteCommands() {
    if(_currentWritCommand == nullptr && getEnableCommands()) {
        if (_powerTargetStatus != nullptr) {
            ESP_LOGD(LogTag().c_str(),"Start writing register power status");
            _currentWritCommand = std::make_unique<WriteRegisterMapping>("002B", 2,*_powerTargetStatus ? "0001" : "0002");
            _powerTargetStatus = nullptr;
        } else if (_limitToSet != nullptr) {
            ESP_LOGD(LogTag().c_str(),"Start writing register limit");
            _currentWritCommand = std::make_unique<WriteRegisterMapping>("0028", 2, String(DeyeUtils::lengthToHexString(*_limitToSet,4).c_str()));
            _limitToSet = nullptr;
        }
    }
}

