// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2025 Thomas Basler and others
 */
#include "TimeoutHelper.h"
#include <Arduino.h>

TimeoutHelper::TimeoutHelper()
{
    timeout = 0;
    startMillis = 0;
}

TimeoutHelper::TimeoutHelper(const uint32_t ms){
    timeout=ms;
    startMillis = millis();
}

void TimeoutHelper::set(const uint32_t ms)
{
    timeout = ms;
    startMillis = millis();
}

void TimeoutHelper::setTimeout(const uint32_t ms)
{
    timeout = ms;
}

void TimeoutHelper::extend(const uint32_t ms)
{
    timeout += ms;
}

void TimeoutHelper::reset()
{
    startMillis = millis();
}

void TimeoutHelper::zero()
{
    startMillis = 0;
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

uint32_t TimeoutHelper::currentMillis() const {
    return startMillis;
}

uint32_t TimeoutHelper::dist() const {
    unsigned long now = millis();
    unsigned long diff = 0;
    if(startMillis < now){
        //milliseconds timer overturn
        diff = now + (std::numeric_limits<unsigned long>::max() - startMillis);
    }else{
        diff = now - startMillis;
    }
    return diff;
}
