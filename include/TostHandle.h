// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TimeoutHelper.h>
#include <iostream>

class TostHandleClass {
public:
    void init();
    void loop();

private:
    TimeoutHelper _lastPublish;

    int lastErrorStatusCode;
    std::string lastErrorMessage;

    long lastTimestamp;
    long lastSuccessfullyTimestamp;

public:
    int getLastTimestamp()const{return lastTimestamp;}
    int getLastSuccessfullyTimestamp()const{return lastSuccessfullyTimestamp;}
    int getLastErrorStatusCode()const{return lastErrorStatusCode;}
    const std::string & getLastErrorMessage()const{return lastErrorMessage;}

};

extern TostHandleClass TostHandle;