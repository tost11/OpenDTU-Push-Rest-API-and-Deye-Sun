#pragma once

#include <cstdint>
#include <ctime>
#include <WString.h>
#include "Updater.h"

class BaseDevInfo : public Updater {
public:
    virtual uint32_t getLastUpdateAll()  const= 0;
    virtual void setLastUpdateAll(const uint32_t lastUpdate) = 0;

    virtual uint32_t getLastUpdateSimple() const = 0;
    virtual void setLastUpdateSimple(const uint32_t lastUpdate) = 0;

    virtual uint16_t getFwBootloaderVersion() const = 0;
    virtual uint16_t getFwBuildVersion() const = 0;
    virtual time_t getFwBuildDateTime() const = 0;
    virtual String getFwBuildDateTimeStr() const = 0;
    virtual uint32_t getHwPartNumber() const = 0;
    virtual String getHwVersion() const = 0;
    virtual uint16_t getMaxPower() const = 0;
    virtual String getHwModelName() const = 0;
private:
};
