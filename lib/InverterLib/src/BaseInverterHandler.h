#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include "inverter/BaseInverter.h"
#include "InverterUtils.h"

template<class InverterType,class StatT,class DevT,class AlarmT,
        typename = std::enable_if<std::is_base_of<BaseStatistics,StatT>::value>,
        typename = std::enable_if<std::is_base_of<BaseDevInfo,DevT>::value>,
        typename = std::enable_if<std::is_base_of<BaseAlarmLog,AlarmT>::value>,
        typename = std::enable_if<std::is_base_of<BasePowerCommand,InverterType>::value>>
class BaseInverterHandler {
public:
    virtual void init() = 0;
    virtual bool isAllRadioIdle() const = 0;
    virtual size_t getNumInverters() const = 0;

    virtual std::shared_ptr<InverterType> getInverterByPos(uint8_t pos) = 0;
    virtual std::shared_ptr<InverterType> getInverterBySerial(uint64_t serial) = 0;
    virtual std::shared_ptr<InverterType> getInverterBySerialString(const String & serial) = 0;

    virtual void removeInverterBySerial(uint64_t serial) = 0;


    uint32_t PollInterval() const {return _pollInterval;};
    void setPollInterval(const uint32_t interval){_pollInterval=interval;}
protected:
    uint32_t _pollInterval = 0;
    uint32_t _lastPoll = 0;

    std::vector<std::shared_ptr<BaseInverterClass>> _baseInverters;

    void performHouseKeeping(){
        // Perform housekeeping of all inverters on day change
        const int8_t currentWeekDay = InverterUtils::getWeekDay();
        static int8_t lastWeekDay = -1;
        if (lastWeekDay == -1) {
            lastWeekDay = currentWeekDay;
        } else {
            if (currentWeekDay != lastWeekDay) {

                for (auto& inv : _baseInverters) {
                    inv->performDailyTask();
                }

                lastWeekDay = currentWeekDay;
            }
        }
    }
public:
};




using BaseInverterHandlerClass = BaseInverterHandler<BaseInverterClass ,BaseStatistics,BaseDevInfo,BaseAlarmLog,BasePowerCommand>;
