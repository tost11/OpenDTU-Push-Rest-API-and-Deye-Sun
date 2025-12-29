
#pragma once

#include <parser/StatisticsParser.h>

#pragma pack(push, 1) // exact fit - no padding
struct BaseData
{
  uint16_t current = 0;
  uint16_t voltage = 0;
  uint16_t power = -1;
  uint32_t dailyEnergy = 0;
  uint32_t totalEnergy = 0;
};

struct InverterData
{
  BaseData grid;
  BaseData pv[4];
  uint16_t gridFreq = 0;
  int16_t inverterTemp = 0;
  uint16_t reactivePower = 0;
  uint16_t powerFactor = 0;
  uint8_t powerLimit = 254;
  uint8_t powerLimitSet = 101; // init with not possible value for startup
  boolean powerLimitSetUpdate = false;
  uint32_t dtuRssi = 0;
  uint32_t wifi_rssi_gateway = 0;
  uint32_t respTimestamp = 1704063600;     // init with start time stamp > 0
  uint32_t lastRespTimestamp = 1704063600; // init with start time stamp > 0
  uint32_t currentTimestamp = 1704063600; // init with start time stamp > 0
};
#pragma pack(pop) //back to whatever the previous packing mode was

class HoymilesWStatisticsParser : public StatisticsParser {
    public:
        HoymilesWStatisticsParser();
        ~HoymilesWStatisticsParser();
    private:
        uint16_t getStaticPayloadSize(){return sizeof(InverterData);}
};