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
    bool sendDtuRestartRequest() override;
    bool resendLimitCommand();
    inverter_type getInverterType() const override;
    void resetStats() override;
    bool supportsPowerDistributionLogic() override;

    // BLE-specific
    void update();
    void setupBle(const char* pin);
    void setEncRand(const uint8_t encRand[16]);
    const uint8_t* getEncRand() const;
    bool hasEncRand() const;
    void startConnection();

    void setPollTime(uint16_t pollTime);

protected:
    void onPollTimeChanged() override;

    // Hook for subclasses to transform the filled PV data before it is committed
    // to the statistics parser (e.g. merge multiple reported ports into one).
    virtual void adjustPvData(HiFlowData &hfData) {}

private:
    uint64_t _serial;
    HiFlowBLEInterface _bleInterface;
    TimeoutHelper _pollTimer;

    // Queued control command (one slot, mirrors the single in-flight BLE request)
    enum class PendingCmd : uint8_t { None, Limit, Power, Restart, DtuRestart };
    PendingCmd _pendingCmd = PendingCmd::None;    // last requested command type
    PendingCmd _cmdInFlightType = PendingCmd::None; // type of the command waiting for ACK
    uint16_t _limitLevel = 0;                     // for Limit: percent * 10 (20..1000)
    bool _powerCmdTurnOn = false;                 // for Power: true = on
    bool _cmdQueued = false;                      // waiting to be sent
    bool _cmdInFlight = false;                    // sent, waiting for ACK
    uint32_t _lastCmdTime = 0;                    // pacing for resends
    static constexpr uint32_t CMD_RETRY_INTERVAL_MS = 10000;

    // Power limit read (via GetConfig after each poll)
    bool _needGetConfig = false;
    bool _configRequestInFlight = false;

    // Inverter serial as reported by RealDataNew (SGSMO) - required for mi_to_sn,
    // differs from the BLE name serial stored in the config
    uint64_t _inverterSerial = 0;

    void processNewData(const HiFlowRealData& data);
};
