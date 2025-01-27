// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Thomas Basler and others
 */
#include "InverterUtils.h"
#include <time.h>

uint8_t InverterUtils::getWeekDay()
{
    time_t raw;
    struct tm info;
    time(&raw);
    localtime_r(&raw, &info);
    return info.tm_mday;
}