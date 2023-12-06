#pragma once

#include "defines.h"
#include "Updater.h"

class BasePowerCommand : public Updater {
public:
    virtual LastCommandSuccess getLastPowerCommandSuccess() = 0;
};
