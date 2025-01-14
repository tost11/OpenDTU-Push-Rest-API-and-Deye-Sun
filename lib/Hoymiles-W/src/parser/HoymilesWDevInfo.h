#pragma once

#include <parser/BaseDevInfo.h>

#define DEV_INFO_SIZE_HOYMILES_W 20

class HoymilesWDevInfo: public BaseDevInfo {
public:

    void clearBuffer();

    void appendFragment(uint8_t offset, uint8_t* payload, uint8_t len);

    uint32_t getLastUpdateAll() const override;

    void setLastUpdateAll(uint32_t lastUpdate) override;

    uint32_t getLastUpdateSimple() const override;

    void setLastUpdateSimple(uint32_t lastUpdate) override;

    uint16_t getFwBootloaderVersion() const override;

    uint16_t getFwBuildVersion() const override;

    time_t getFwBuildDateTime() const override;

    String getFwBuildDateTimeStr() const override;

    uint32_t getHwPartNumber() const override;

    String getHwVersion() const override;

    uint16_t getMaxPower() const override;

    String getHwModelName() const override;

    void setMaxPowerDevider(uint8_t dev);

    void setHardwareVersion(const String & version);
    void setHardwareModel(const String & model);
    void setMaxPower(const uint16_t maxPower);
private:
    uint8_t _payloadDevInfo[DEV_INFO_SIZE_HOYMILES_W] = {};
    uint8_t _devInfoLength = 0;

    uint8_t _maxPowerDevider;
    String _hardwareVersion;
    String _hardwareModel;
    uint16_t _maxPower;
};