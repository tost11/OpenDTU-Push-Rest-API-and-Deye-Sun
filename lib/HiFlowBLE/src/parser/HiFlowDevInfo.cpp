#include "HiFlowDevInfo.h"
#include <cstring>

void HiFlowDevInfo::clearBuffer()
{
}

void HiFlowDevInfo::appendFragment(uint8_t offset, uint8_t* payload, uint8_t len)
{
    (void)offset;
    (void)payload;
    (void)len;
}

uint32_t HiFlowDevInfo::getLastUpdateAll() const
{
    return _lastUpdate;
}

void HiFlowDevInfo::setLastUpdateAll(uint32_t lastUpdate)
{
    _lastUpdate = lastUpdate;
}

uint32_t HiFlowDevInfo::getLastUpdateSimple() const
{
    return _lastUpdate;
}

void HiFlowDevInfo::setLastUpdateSimple(uint32_t lastUpdate)
{
    _lastUpdate = lastUpdate;
}

uint16_t HiFlowDevInfo::getFwBootloaderVersion() const
{
    return 0;
}

uint16_t HiFlowDevInfo::getFwBuildVersion() const
{
    return 0;
}

time_t HiFlowDevInfo::getFwBuildDateTime() const
{
    return 0;
}

String HiFlowDevInfo::getFwBuildDateTimeStr() const
{
    return "";
}

uint32_t HiFlowDevInfo::getHwPartNumber() const
{
    return 0;
}

String HiFlowDevInfo::getHwVersion() const
{
    return "1.0";
}

uint16_t HiFlowDevInfo::getMaxPower() const
{
    return _maxPower;
}

String HiFlowDevInfo::getHwModelName() const
{
    return _hardwareModel;
}

void HiFlowDevInfo::setHardwareModel(const String& model)
{
    _hardwareModel = model;
}

void HiFlowDevInfo::setMaxPower(uint16_t maxPower)
{
    _maxPower = maxPower;
}

void HiFlowDevInfo::setLastUpdate(uint32_t lastUpdate)
{
    _lastUpdate = lastUpdate;
}
