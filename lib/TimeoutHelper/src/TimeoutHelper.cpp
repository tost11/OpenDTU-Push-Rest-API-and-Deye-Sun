// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2025 Thomas Basler and others
 */
#include "TimeoutHelper.h"
#include <Arduino.h>

TimeoutHelper::TimeoutHelper()
{
    timeout = 0;
    startMillis = millis();
    zeroStartup = false;
}

TimeoutHelper::TimeoutHelper(const uint32_t ms){
    timeout=ms;
    startMillis = millis();
    zeroStartup = false;
}

void TimeoutHelper::set(const uint32_t ms)
{
    timeout = ms;
    startMillis = millis();
    zeroStartup = false;
}

void TimeoutHelper::setTimeout(const uint32_t ms)
{
    timeout = ms;
}

uint32_t TimeoutHelper::getTimeout(){
    return timeout;
}

void TimeoutHelper::extend(const uint32_t ms)
{
    timeout += ms;
}

void TimeoutHelper::reset()
{
    startMillis = millis();
    zeroStartup = false;
}

void TimeoutHelper::zero()
{
    zeroStartup = true;
}

bool TimeoutHelper::occured() const
{
    if(zeroStartup){
        return true;
    }
    return dist() > timeout;
}

uint32_t TimeoutHelper::currentMillis() const {
    return startMillis;
}

uint32_t TimeoutHelper::dist() const {
    //rollover is handled correctly in this calculation
    return millis() - startMillis;
}
