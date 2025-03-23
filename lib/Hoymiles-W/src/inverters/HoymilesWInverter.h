#pragma once

#include "types.h"
#include <inverter/BaseNetworkInverter.h>
#include "parser/HoymilesWDevInfo.h"
#include "parser/HoymilesWAlarmLog.h"
#include "parser/HoymilesWStatisticsParser.h"
#include "parser/PowerCommandParser.h"
#include <Arduino.h>
#include <cstdint>
#include <list>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <TimeoutHelper.h>
#include "../dtuInterface.h"

class HoymilesWInverter : public BaseNetworkInverter<HoymilesWStatisticsParser,HoymilesWDevInfo,HoymilesWAlarmLog,PowerCommandParser> {
public:
    explicit HoymilesWInverter(uint64_t serial);
    virtual ~HoymilesWInverter() = default;

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
protected:
    virtual void hostOrPortUpdated() override;
private:
    uint64_t _serial;

    DTUInterface _dtuInterface;

    TimeoutHelper _dataUpdateTimer;
    TimeoutHelper _dataStatisticTimer;
    bool _clearBufferOnDisconnect;

    uint16_t getInternalPollTime();
protected:
};