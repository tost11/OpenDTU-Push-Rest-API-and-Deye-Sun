#pragma once

#include <inverter/BaseInverter.h>
#include "../parser/HiFlowStatisticsParser.h"
#include "../parser/HiFlowDevInfo.h"
#include "../parser/HiFlowPowerCommand.h"
#include <parser/DefaultAlarmLog.h>
#include <TimeoutHelper.h>
#include "../HiFlowBLEInterface.h"

class HiFlowInverter : public BaseInverter<HiFlowStatisticsParser, HiFlowDevInfo, DefaultAlarmLog, HiFlowPowerCommand> {
public:
    explicit HiFlowInverter(uint64_t serial);
    virtual ~HiFlowInverter() = default;

    // BaseInverter pure virtuals
    uint64_t serial() const override;
    String typeName() const override;
    bool isProducing() override;
    bool isReachable() override;
    bool sendActivePowerControlRequest(float limit, PowerLimitControlType type) override;
    bool resendPowerControlRequest() override;
    bool sendRestartControlRequest() override;
    bool sendPowerControlRequest(bool turnOn) override;
    inverter_type getInverterType() const override;
    void resetStats() override;
    bool supportsPowerDistributionLogic() override;

    // BLE-specific
    void update();
    void setBleAddress(const char* mac);
    void setSerialNumber(const char* sn);
    void setEncRand(const uint8_t encRand[16]);
    const uint8_t* getEncRand() const;
    bool hasEncRand() const;
    void startConnection();

    void setPollTime(uint16_t pollTime);

protected:
    void onPollTimeChanged() override;

private:
    uint64_t _serial;
    HiFlowBLEInterface _bleInterface;
    TimeoutHelper _pollTimer;

    void processNewData(const HiFlowRealData& data);
};
