//
// Created by lukas on 29.03.25.
//

#pragma once

#include <cstdio>

struct HoymilesWConnectionStatistics{
    // TX Request Data
    uint32_t SendRequests;
    uint32_t SuccessfulRequests;

    uint32_t Disconnects;
    uint32_t ConnectionTimeouts;
};
