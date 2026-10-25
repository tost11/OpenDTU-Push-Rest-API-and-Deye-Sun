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
    return getLastUpdate();
}

void HiFlowDevInfo::setLastUpdateAll(uint32_t lastUpdate)
{
    setLastUpdate(lastUpdate);
}

uint32_t HiFlowDevInfo::getLastUpdateSimple() const
{
    return getLastUpdate();
}

void HiFlowDevInfo::setLastUpdateSimple(uint32_t lastUpdate)
{
    setLastUpdate(lastUpdate);
}

uint16_t HiFlowDevInfo::getFwBootloaderVersion() const
{
    return 0;
}

uint16_t HiFlowDevInfo::getFwBuildVersion() const
{
    return _fwBuildVersion;
}

uint16_t HiFlowDevInfo::getDtuFwVersion() const
{
    return _dtuFwVersion;
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

uint16_t HiFlowDevInfo::getMaxPowerDevice() const
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

void HiFlowDevInfo::setFwBuildVersion(uint16_t version)
{
    _fwBuildVersion = version;
}

void HiFlowDevInfo::setDtuFwVersion(uint16_t version)
{
    _dtuFwVersion = version;
}
