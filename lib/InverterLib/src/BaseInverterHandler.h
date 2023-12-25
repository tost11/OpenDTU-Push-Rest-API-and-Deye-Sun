#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include "BaseInverter.h"

template<class InverterType,class StatT,class DevT,class SysT,class AlarmT,class GridT,
        typename = std::enable_if_t<std::is_base_of<BaseStatistics,StatT>::value>,
        typename = std::enable_if<std::is_base_of<BaseDevInfo,DevT>::value>,
        typename = std::enable_if<std::is_base_of<BaseSystemConfigPara,SysT>::value>,
        typename = std::enable_if<std::is_base_of<BaseAlarmLog,AlarmT>::value>,
        typename = std::enable_if<std::is_base_of<BaseGridProfile,GridT>::value>,
        typename = std::enable_if<std::is_base_of<BasePowerCommand,InverterType>::value>>
class BaseInverterHandler {
public:
    virtual void init() = 0;
    virtual bool isAllRadioIdle() const = 0;
    virtual size_t getNumInverters() const = 0;

    virtual std::shared_ptr<InverterType> getInverterByPos(uint8_t pos) = 0;
    virtual std::shared_ptr<InverterType> getInverterBySerial(uint64_t serial) = 0;

    virtual void removeInverterBySerial(uint64_t serial) = 0;


    uint32_t PollInterval() const {return _pollInterval;};
    void setPollInterval(const uint32_t interval){_pollInterval=interval;}
protected:
    uint32_t _pollInterval = 0;
    uint32_t _lastPoll = 0;

public:
};

using BaseInverterHandlerClass = BaseInverterHandler<BaseInverterClass ,BaseStatistics,BaseDevInfo,BaseSystemConfigPara,BaseAlarmLog,BaseGridProfile,BasePowerCommand>;
