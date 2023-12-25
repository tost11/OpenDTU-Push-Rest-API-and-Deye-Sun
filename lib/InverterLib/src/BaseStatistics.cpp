
#include "BaseStatistics.h"

std::list<ChannelType_t> BaseStatistics::getChannelTypes() const
{
    return {
            TYPE_AC,
            TYPE_DC,
            TYPE_INV
    };
}

const char* BaseStatistics::getChannelTypeName(ChannelType_t type) const
{
    return channelsTypes[type];
}

uint16_t BaseStatistics::getStringMaxPower(uint8_t channel) const
{
    return _stringMaxPower[channel];
}

void BaseStatistics::setStringMaxPower(uint8_t channel, uint16_t power)
{
    if (channel < sizeof(_stringMaxPower) / sizeof(_stringMaxPower[0])) {
        _stringMaxPower[channel] = power;
    }
}

uint32_t BaseStatistics::getLastUpdateFromInternal() const
{
    return _lastUpdateFromInternal;
}

void BaseStatistics::setLastUpdateFromInternal(uint32_t lastUpdate)
{
    _lastUpdateFromInternal = lastUpdate;
}