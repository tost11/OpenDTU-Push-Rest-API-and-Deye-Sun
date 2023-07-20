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
    long lastTimestamp;
    long lastSuccessfullyTimestamp;
    std::string lastMessage;

public:
    int getLastSuccessfullyTimestamp()const{return lastSuccessfullyTimestamp;}
    int getLastErrorStatusCode()const{return lastErrorStatusCode;}

};

extern TostHandleClass TostHandle;