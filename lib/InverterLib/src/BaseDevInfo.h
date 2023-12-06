#pragma once

#include <cstdint>
#include <ctime>
#include <WString.h>
#include "Updater.h"

class BaseDevInfo : public Updater {
public:
    virtual uint32_t getLastUpdateAll() = 0;
    virtual void setLastUpdateAll(uint32_t lastUpdate) = 0;

    virtual uint32_t getLastUpdateSimple() = 0;
    virtual void setLastUpdateSimple(uint32_t lastUpdate) = 0;

    virtual uint16_t getFwBootloaderVersion() = 0;
    virtual uint16_t getFwBuildVersion() = 0;
    virtual time_t getFwBuildDateTime() = 0;
    virtual uint32_t getHwPartNumber() = 0;
    virtual String getHwVersion() = 0;
    virtual uint16_t getMaxPower() = 0;
    virtual String getHwModelName() = 0;
private:
};
