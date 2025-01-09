//
// Created by lukas on 24.11.23.
//

#pragma once

#include "defines.h"
#include "Updater.h"

class BaseSystemConfigPara : public Updater {
public:
    virtual float getLimitPercent() const = 0;
    virtual LastCommandSuccess getLastLimitCommandSuccess() const = 0;
};

