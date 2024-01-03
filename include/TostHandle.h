// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TimeoutHelper.h>
#include <iostream>
#include <Hoymiles.h>
#include "Configuration.h"
#include <TaskSchedulerDeclarations.h>

class TostHandleClass {
public:
    void init(Scheduler& scheduler);

private:
    Task _loopTask;
    void loop();

    TimeoutHelper _lastPublish;

    uint32_t _lastPublishStats[INV_MAX_COUNT];

    int lastErrorStatusCode;
    std::string lastErrorMessage;

    long lastErrorTimestamp;
    long lastSuccessfullyTimestamp;

    FieldId_t _publishFields[14] = {
            FLD_UDC,
            FLD_IDC,
            FLD_PDC,
            FLD_YD,
            FLD_YT,
            FLD_UAC,
            FLD_IAC,
            FLD_PAC,
            FLD_F,
            FLD_T,
            FLD_PF,
            FLD_EFF,
            FLD_IRR,
            FLD_Q
        };

public:
    unsigned long getLastErrorTimestamp()const{return lastErrorTimestamp;}
    unsigned long getLastSuccessfullyTimestamp()const{return lastSuccessfullyTimestamp;}
    int getLastErrorStatusCode()const{return lastErrorStatusCode;}
    const std::string & getLastErrorMessage()const{return lastErrorMessage;}

};

extern TostHandleClass TostHandle;