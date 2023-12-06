#pragma once

#include "BaseDevInfo.h"

class DeyeDevInfo: public BaseDevInfo {

public:
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
};