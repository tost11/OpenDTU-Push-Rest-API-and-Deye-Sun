// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "TimeoutHelper.h"
#include <Arduino.h>

TimeoutHelper::TimeoutHelper()
{
    timeout = 0;
    startMillis = 0;
}

void TimeoutHelper::set(const uint32_t ms)
{
    timeout = ms;
    startMillis = millis();
}

void TimeoutHelper::extend(const uint32_t ms)
{
    timeout += ms;
}

void TimeoutHelper::reset()
{
    startMillis = millis();
}

bool TimeoutHelper::occured() const
{
    unsigned long now = millis();
    unsigned long diff = 0;
    if(startMillis < now){
        //milliseconds timer overurne
        diff = now + (std::numeric_limits<unsigned long>::max() - startMillis);
    }else{
        diff = now - startMillis;
    }
    return diff > timeout;
}