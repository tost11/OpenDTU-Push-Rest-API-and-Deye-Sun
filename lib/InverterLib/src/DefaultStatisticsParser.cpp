#include "DefaultStatisticsParser.h"

DefaultStatisticsParser::DefaultStatisticsParser(){
    Serial.println("Creation of buffer working");
    _payloadStatistic = new uint8_t[STATISTIC_PACKET_SIZE];
    clearBuffer();
}

DefaultStatisticsParser::~DefaultStatisticsParser(){
    delete [] _payloadStatistic;
}