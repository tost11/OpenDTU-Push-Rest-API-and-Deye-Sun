//
// Created by lukas on 31.05.25.
//

#pragma once

#include <parser/DefaultStatisticsParser.h>
#include "parser/DeyeDevInfo.h"
#include "parser/DeyeAlarmLog.h"
#include "parser/PowerCommandParser.h"
#include <inverter/BaseNetworkInverter.h>
#include <TimeoutHelper.h>
#include <optional>
#include <future>

// Forward declaration
struct RestResponse;

enum deye_inverter_type {
    Deye_Sun_At_Commands = 0,
    Deye_Sun_Custom_Modbus,

    //Coming Soon
    //Deye_Sun_Modbus,

    Deye_Sun_Inverter_Count
};

struct WriteRegisterMapping{
    String writeRegister;
    uint8_t length;
    String valueToWrite;

    WriteRegisterMapping(const String &writeRegister, uint8_t length, const String &valueToWrite):
            writeRegister(writeRegister),
            length(length),
            valueToWrite(valueToWrite){}
};

class DeyeInverter : public BaseNetworkInverter<DefaultStatisticsParser,DeyeDevInfo,DeyeAlarmLog,PowerCommandParser> {
private:
    uint64_t _serial;

    // Firmware version fetching
    std::optional<std::future<RestResponse>> _firmwareVersionFuture;
    TimeoutHelper _timerFirmwareVersionFetch;
    static const uint32_t TIMER_FIRMWARE_VERSION_FETCH_SUCCESS = 15 * 60 * 1000; // 15 minutes
    static const uint32_t TIMER_FIRMWARE_VERSION_FETCH_RETRY = 30 * 1000;       // 30 seconds
    std::mutex _restartCommandMutex;

    // Restart command tracking
    std::optional<std::future<RestResponse>> _restartCommandFuture;

protected:
    void handleDeyeDayCorrection();
    void checkAndFetchFirmwareVersion();
    void checkRestartCommandResult();
    String parseCoverVerFromHtml(const String& htmlBody);
    String parseCoverMidFromHtml(const String& htmlBody);

    std::unique_ptr<bool> _powerTargetStatus;
    std::unique_ptr<uint16_t > _limitToSet;

    std::unique_ptr<WriteRegisterMapping> _currentWritCommand;

    void checkForNewWriteCommands();
public:
    explicit DeyeInverter(uint64_t serial);

    uint64_t serial() const override;

    virtual deye_inverter_type getDeyeInverterType() const = 0;

    static String serialToModel(uint64_t serial);

    virtual void update() = 0;

    bool isProducing() override;

    String typeName() const override;

    inverter_type getInverterType() const override;

    bool sendActivePowerControlRequest(float limit, const PowerLimitControlType type) override;
    bool sendPowerControlRequest(bool turnOn) override;
    bool resendPowerControlRequest() override;
    bool sendRestartControlRequest() override;
};
