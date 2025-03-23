#pragma once

#include <cstdint>
#include <WString.h>
#include <memory>
#include "../parser/BaseStatistics.h"
#include "../parser/StatisticsParser.h"
#include "../parser/BaseDevInfo.h"
#include "../parser/SystemConfigParaParser.h"
#include "defines.h"
#include "../parser/BaseAlarmLog.h"
#include "../parser/GridProfileParser.h"
#include "../parser/BasePowerCommand.h"

#define MAX_NAME_LENGTH 32

template<class StatT,class DevT,class AlarmT,class PowerT,
        typename = std::enable_if<std::is_base_of<BaseStatistics,StatT>::value>,
        typename = std::enable_if<std::is_base_of<BaseDevInfo,DevT>::value>,      
        typename = std::enable_if<std::is_base_of<BaseAlarmLog,AlarmT>::value>,
        typename = std::enable_if<std::is_base_of<BasePowerCommand,PowerT>::value>>
class BaseInverter {
public:
    BaseInverter(){
        _gridProfileParser = std::make_unique<GridProfileParser>();
        _systemConfigParaParser = std::make_unique<SystemConfigParaParser>();
    }
    ~BaseInverter() = default;

    AlarmT* getEventLog(){return _alarmLogParser.get();}
    DevT* getDevInfo(){return _devInfoParser.get();}
    GridProfileParser* getGridProfileParser(){return _gridProfileParser.get();}
    PowerT* getPowerCommand(){return _powerCommandParser.get();}
    StatT* getStatistics(){return _statisticsParser.get();}
    SystemConfigParaParser* getSystemConfigParaParser(){return _systemConfigParaParser.get();}

    virtual uint64_t serial() const = 0;
    virtual String typeName() const = 0;

    virtual bool isProducing() = 0;
    virtual bool isReachable() = 0;

    virtual bool sendActivePowerControlRequest(float limit,const PowerLimitControlType type) = 0;
    virtual bool resendPowerControlRequest() = 0;
    virtual bool sendRestartControlRequest() = 0;
    virtual bool sendPowerControlRequest(const bool turnOn) = 0;
    virtual inverter_type getInverterType() const = 0;
protected:

    String _serialString;
    char _name[MAX_NAME_LENGTH] = "";

    bool _enablePolling = true;
    bool _enableCommands = true;

    uint8_t _reachableThreshold = 3;
    uint16_t _pollTime = 60;//should never be used but when seen something is wrong

    bool _zeroValuesIfUnreachable = false;
    bool _zeroYieldDayOnMidnight = false;

    bool _clearEventlogOnMidnight = false;

    std::unique_ptr<StatT> _statisticsParser;
    std::unique_ptr<DevT> _devInfoParser;
    std::unique_ptr<SystemConfigParaParser> _systemConfigParaParser;
    std::unique_ptr<GridProfileParser> _gridProfileParser;
    std::unique_ptr<AlarmT> _alarmLogParser;
    std::unique_ptr<PowerT> _powerCommandParser;

public:

    // This feature will limit the AC output instead of limiting the DC inputs.
    virtual bool supportsPowerDistributionLogic() = 0;

    void setReachableThreshold(const uint8_t threshold)
    {
        _reachableThreshold = threshold;
    }

    uint8_t getReachableThreshold() const
    {
        return _reachableThreshold;
    }

    void setPollTime(const uint16_t pollTime)
    {
        _pollTime = pollTime;
    }

    uint16_t getPollTime() const
    {
        return _pollTime;
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

    virtual void setEnableCommands(const bool enabled)
    {
        _enableCommands = enabled;
    }

    bool getEnableCommands() const
    {
        return _enableCommands;
    }

    void setClearEventlogOnMidnight(const bool enabled)
    {
        _clearEventlogOnMidnight = enabled;
    }

    bool getClearEventlogOnMidnight() const
    {
        return _clearEventlogOnMidnight;
    }

    virtual void performDailyTask()
    {
        // Have to reset the offets first, otherwise it will
        // Substract the offset from zero which leads to a high value
        getStatistics()->resetYieldDayCorrection();
        if (getZeroYieldDayOnMidnight()) {
            getStatistics()->zeroDailyData();
        }
        if (getClearEventlogOnMidnight()) {
            getEventLog()->clearBuffer();
        }
    }
};

using BaseInverterClass = BaseInverter<BaseStatistics,BaseDevInfo,BaseAlarmLog,BasePowerCommand>;