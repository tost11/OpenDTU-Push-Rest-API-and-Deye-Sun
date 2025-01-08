
#pragma once

#include "StatisticsParser.h"
#include "RealtimeDataNew.pb.h"

#define STATISTIC_PACKET_SIZE (7 * 16)

#pragma pack(push, 1) // exact fit - no padding
struct FetchedDataSample{
  SGSMO gridData;
  PvMO pvData[4];
};
#pragma pack(pop) //back to whatever the previous packing mode was 

class HoymilesWStatisticsParser : public StatisticsParser {
    public:
        HoymilesWStatisticsParser();
        ~HoymilesWStatisticsParser();
    private:
        uint16_t getStaticPayloadSize(){return sizeof(FetchedDataSample);}
};