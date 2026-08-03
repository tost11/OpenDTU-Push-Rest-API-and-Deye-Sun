#pragma once

#include <parser/StatisticsParser.h>

// Packed struct that holds the data received from HiFlow BLE inverter.
// byteAssign_t table maps fields from this struct to the statistics system.
#pragma pack(push, 1)
struct HiFlowData {
    // AC side
    uint16_t acCurrent = 0;      // A * 100 (offset 0)
    uint16_t acVoltage = 0;      // V * 10  (offset 2)
    uint16_t acPower = 0;        // W * 10  (offset 4)
    uint32_t acEnergyDaily = 0;  // Wh      (offset 6)
    uint32_t acEnergyTotal = 0;  // Wh (stored as Wh, divided by 1000 for kWh) (offset 10)
    // PV inputs (up to 4)
    struct {
        uint16_t voltage = 0;    // V * 10
        uint16_t current = 0;    // A * 100
        uint16_t power = 0;      // W * 10
        uint32_t energyDaily = 0;// Wh
        uint32_t energyTotal = 0;// Wh
    } pv[4];                     // pv[0] at offset 14, each 14 bytes
    // Grid
    uint16_t frequency = 0;      // Hz * 100 (offset 70)
    int16_t  temperature = 0;    // C * 10   (offset 72)
    int16_t  reactivePower = 0;  // var      (offset 74)
    int16_t  powerFactor = 0;    // * 1000   (offset 76)
};
#pragma pack(pop)

// Offsets within HiFlowData struct
static constexpr uint8_t HF_OFF_AC_CURRENT     = 0;
static constexpr uint8_t HF_OFF_AC_VOLTAGE     = 2;
static constexpr uint8_t HF_OFF_AC_POWER       = 4;
static constexpr uint8_t HF_OFF_AC_ENERGY_DAY  = 6;
static constexpr uint8_t HF_OFF_AC_ENERGY_TOT  = 10;
static constexpr uint8_t HF_OFF_PV_BASE        = 14;  // Each PV port is 14 bytes
static constexpr uint8_t HF_PV_SIZE            = 14;
static constexpr uint8_t HF_OFF_PV_VOLTAGE     = 0;
static constexpr uint8_t HF_OFF_PV_CURRENT     = 2;
static constexpr uint8_t HF_OFF_PV_POWER       = 4;
static constexpr uint8_t HF_OFF_PV_ENERGY_DAY  = 6;
static constexpr uint8_t HF_OFF_PV_ENERGY_TOT  = 10;
static constexpr uint8_t HF_OFF_FREQUENCY      = 70;
static constexpr uint8_t HF_OFF_TEMPERATURE    = 72;
static constexpr uint8_t HF_OFF_REACTIVE_PWR   = 74;
static constexpr uint8_t HF_OFF_POWER_FACTOR   = 76;

class HiFlowStatisticsParser : public StatisticsParser {
public:
    HiFlowStatisticsParser();
    ~HiFlowStatisticsParser();

private:
    uint16_t getStaticPayloadSize() override { return sizeof(HiFlowData); }
};
