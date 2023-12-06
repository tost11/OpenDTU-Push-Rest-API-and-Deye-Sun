#include "Updater.h"

uint32_t Updater::getLastUpdate()
{
    return _lastUpdate;
}

void Updater::setLastUpdate(uint32_t lastUpdate)
{
    _lastUpdate = lastUpdate;
}