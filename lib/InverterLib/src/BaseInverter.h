#pragma once

#include <cstdint>
#include <WString.h>
#include <memory>
#include "BaseStatistics.h"
#include "BaseDevInfo.h"
#include "BaseSystemConfigPara.h"
#include "defines.h"
#include "BaseAlarmLog.h"
#include "BaseGridProfile.h"
#include "BasePowerCommand.h"

#define MAX_NAME_LENGTH 32

template<class StatT,class DevT,class SysT,class AlarmT,class GridT,class PowerT,
        typename = std::enable_if_t<std::is_base_of<BaseStatistics,StatT>::value>,
        typename = std::enable_if<std::is_base_of<BaseDevInfo,DevT>::value>,
        typename = std::enable_if<std::is_base_of<BaseSystemConfigPara,SysT>::value>,
        typename = std::enable_if<std::is_base_of<BaseAlarmLog,AlarmT>::value>,
        typename = std::enable_if<std::is_base_of<BaseGridProfile,GridT>::value>,
        typename = std::enable_if<std::is_base_of<BasePowerCommand,PowerT>::value>>
class BaseInverter {
public:
    BaseInverter() = default;
    ~BaseInverter() = default;

    AlarmT* EventLog(){return _alarmLogParser.get();}
    DevT* DevInfo(){return _devInfoParser.get();}
    GridT* GridProfile(){return _gridProfileParser.get();}
    PowerT* PowerCommand(){return _powerCommandParser.get();}
    StatT* Statistics(){return _statisticsParser.get();}
    SysT* SystemConfigPara(){return _systemConfigParaParser.get();}

    virtual uint64_t serial() const = 0;
    virtual String typeName() const = 0;

    virtual bool isProducing() = 0;
    virtual bool isReachable() = 0;

    virtual bool sendActivePowerControlRequest(float limit,const PowerLimitControlType type) = 0;
    virtual bool resendPowerControlRequest() = 0;
    virtual bool sendRestartControlRequest() = 0;
    virtual bool sendPowerControlRequest(const bool turnOn) = 0;
    virtual inverter_type getInverterType() = 0;
protected:

    String _serialString;
    char _name[MAX_NAME_LENGTH] = "";

    bool _enablePolling = true;
    bool _enableCommands = true;

    uint8_t _reachableThreshold = 3;

    bool _zeroValuesIfUnreachable = false;
    bool _zeroYieldDayOnMidnight = false;

    std::unique_ptr<StatT> _statisticsParser;
    std::unique_ptr<DevT> _devInfoParser;
    std::unique_ptr<SysT> _systemConfigParaParser;
    std::unique_ptr<GridT> _gridProfileParser;
    std::unique_ptr<AlarmT> _alarmLogParser;
    std::unique_ptr<PowerT> _powerCommandParser;

public:
    void setReachableThreshold(const uint8_t threshold)
    {
        _reachableThreshold = threshold;
    }

    uint8_t getReachableThreshold() const
    {
        return _reachableThreshold;
    }

    void setZeroValuesIfUnreachable(bool enabled)
    {
        _zeroValuesIfUnreachable = enabled;
    }

    bool getZeroValuesIfUnreachable() const
    {
        return _zeroValuesIfUnreachable;
    }

    void setZeroYieldDayOnMidnight(const bool enabled)
    {
        _zeroYieldDayOnMidnight = enabled;
    }

    bool getZeroYieldDayOnMidnight() const
    {
        return _zeroYieldDayOnMidnight;
    }


    const String& serialString() const
    {
        return _serialString;
    }

    void setName(const char* name)
    {
        uint8_t len = strlen(name);
        if (len + 1 > MAX_NAME_LENGTH) {
            len = MAX_NAME_LENGTH - 1;
        }
        strncpy(_name, name, len);
        _name[len] = '\0';
    }

    const char* name() const
    {
        return _name;
    }

    void setEnablePolling(const bool enabled)
    {
        _enablePolling = enabled;
    }

    bool getEnablePolling() const
    {
        return _enablePolling;
    }

    void setEnableCommands(const bool enabled)
    {
        _enableCommands = enabled;
    }

    bool getEnableCommands() const
    {
        return _enableCommands;
    }
};

using BaseInverterClass = BaseInverter<BaseStatistics,BaseDevInfo,BaseSystemConfigPara,BaseAlarmLog,BaseGridProfile,BasePowerCommand>;