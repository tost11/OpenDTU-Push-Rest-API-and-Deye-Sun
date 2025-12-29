#include "DefaultStatisticsParser.h"

DefaultStatisticsParser::DefaultStatisticsParser(){
    _payloadStatistic = new uint8_t[STATISTIC_PACKET_SIZE];
    clearBuffer();
}

DefaultStatisticsParser::~DefaultStatisticsParser(){
    delete [] _payloadStatistic;
}