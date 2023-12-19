#pragma once

#include "BaseDevInfo.h"

#define DEV_INFO_SIZE_DEYE 20

class DeyeDevInfo: public BaseDevInfo {

public:

    void clearBuffer();

    void appendFragment(uint8_t offset, uint8_t* payload, uint8_t len);

    uint32_t getLastUpdateAll() override;

    void setLastUpdateAll(uint32_t lastUpdate) override;

    uint32_t getLastUpdateSimple() override;

    void setLastUpdateSimple(uint32_t lastUpdate) override;

    uint16_t getFwBootloaderVersion() override;

    uint16_t getFwBuildVersion() override;

    time_t getFwBuildDateTime() override;

    uint32_t getHwPartNumber() override;

    String getHwVersion() override;

    uint16_t getMaxPower() override;

    String getHwModelName() override;

    void setMaxPowerDevider(uint8_t dev);

    void setHardwareVersion(const String & version);
    void setHardwareModel(const String & model);
private:
    uint8_t _payloadDevInfo[DEV_INFO_SIZE_DEYE] = {};
    uint8_t _devInfoLength = 0;

    uint8_t _maxPowerDevider;
    String _hardwareVersion;
    String _hardwareModel;
};