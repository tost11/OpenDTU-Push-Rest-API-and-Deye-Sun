
#pragma once

#include "StatisticsParser.h"

//#define STATISTIC_PACKET_SIZE (7 * 16)
#define STATISTIC_PACKET_SIZE (7 * 16)

class DefaultStatisticsParser : public StatisticsParser {
    public:
        DefaultStatisticsParser();
        ~DefaultStatisticsParser();
    private:
        uint16_t getStaticPayloadSize(){return STATISTIC_PACKET_SIZE;}
};