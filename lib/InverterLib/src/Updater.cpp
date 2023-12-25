#include "Updater.h"

uint32_t Updater::getLastUpdate() const
{
    return _lastUpdate;
}

void Updater::setLastUpdate(const uint32_t lastUpdate)
{
    _lastUpdate = lastUpdate;
}