#pragma once

#include <parser/BaseDevInfo.h>
#include <WString.h>

class HiFlowDevInfo : public BaseDevInfo {
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

    void setHardwareModel(const String& model);
    void setMaxPower(uint16_t maxPower);
    void setLastUpdate(uint32_t lastUpdate);

private:
    String _hardwareModel = "HiFlow Pro";
    uint16_t _maxPower = 800; // Default for HMS-800-2WB
    uint32_t _lastUpdate = 0;
};
