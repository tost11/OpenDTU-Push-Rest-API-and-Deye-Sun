#pragma once

#include <inverter/BaseNetworkInverter.h>
#include "parser/HoymilesWDevInfo.h"
#include <parser/DefaultAlarmLog.h>
#include "parser/HoymilesWStatisticsParser.h"
#include "parser/PowerCommandParser.h"
#include <cstdint>
#include <TimeoutHelper.h>
#include "../dtuInterface.h"

class HoymilesWInverter : public BaseNetworkInverter<HoymilesWStatisticsParser,HoymilesWDevInfo,DefaultAlarmLog,PowerCommandParser> {
public:
    explicit HoymilesWInverter(uint64_t serial);
    virtual ~HoymilesWInverter() = default;

    struct HoymilesWConnectionStatistics ConnectionStatistics = {};

    uint64_t serial() const override;

    String typeName() const override;

    bool isProducing() override;

    bool isReachable() override;

    bool sendActivePowerControlRequest(float limit, PowerLimitControlType type) override;

    bool resendPowerControlRequest() override;

    bool sendRestartControlRequest() override;

    bool sendPowerControlRequest(bool turnOn) override;

    void update();

    inverter_type getInverterType() const override;

    void setEnableCommands(const bool enabled) override;

    void startConnection();

    void swapBuffers(const InverterData *data);

    bool supportsPowerDistributionLogic() override;

    void onPollTimeChanged() override;

    void resetStats() override;

protected:
    virtual void hostOrPortUpdated() override;

    String LogTag() override;

private:
    uint64_t _serial;

    DTUInterface _dtuInterface;

    TimeoutHelper _dataUpdateTimer;
    TimeoutHelper _dataStatisticTimer;
    bool _changeCheckedClearOnDisconnect;

    uint64_t getInternalPollTime();
protected:
};