//
// Created by lukas on 24.11.23.
//

#ifndef OPENDTU_DEYE_SUN_BASESYSTEMCONFIGPARA_H
#define OPENDTU_DEYE_SUN_BASESYSTEMCONFIGPARA_H


#include <cstdint>
#include "defines.h"
#include "Updater.h"

class BaseSystemConfigPara : public Updater {
public:
    virtual float getLimitPercent() = 0;
    virtual LastCommandSuccess getLastLimitCommandSuccess() = 0;
};


#endif //OPENDTU_DEYE_SUN_BASESYSTEMCONFIGPARA_H
