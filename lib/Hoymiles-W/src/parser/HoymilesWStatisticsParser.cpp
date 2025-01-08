#include "HoymilesWStatisticsParser.h"

HoymilesWStatisticsParser::HoymilesWStatisticsParser(){
    _payloadStatistic = new uint8_t[sizeof(FetchedDataSample)];
    clearBuffer();
}

HoymilesWStatisticsParser::~HoymilesWStatisticsParser(){
    delete [] _payloadStatistic;
}