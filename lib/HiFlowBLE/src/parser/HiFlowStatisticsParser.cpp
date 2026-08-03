#include "HiFlowStatisticsParser.h"

HiFlowStatisticsParser::HiFlowStatisticsParser()
{
    _payloadStatistic = new uint8_t[sizeof(HiFlowData)];
    clearBuffer();
}

HiFlowStatisticsParser::~HiFlowStatisticsParser()
{
    delete[] _payloadStatistic;
}
