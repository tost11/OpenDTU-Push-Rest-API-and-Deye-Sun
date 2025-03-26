#pragma once

#include <cstdint>

struct ConnectionStatistics{
    // TX Request Data
    uint32_t SendRequests;
    uint32_t SuccessfulRequests;

    uint32_t Disconnects;
    uint32_t ConnectionTimeouts;
};