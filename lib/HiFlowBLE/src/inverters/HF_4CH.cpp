#include "HF_4CH.h"
#include "../parser/HiFlowStatisticsParser.h"

// byteAssign_t table for 4-channel HiFlow Pro (HMS-1600-4WB)
// Maps fields from HiFlowData packed struct to the statistics system.
static const byteAssign_t byteAssignment_HF_4CH[] = {
    // DC Channel 0 (PV port 0)
    { TYPE_DC, CH0, FLD_UDC, UNIT_V,   HF_OFF_PV_BASE + 0 * HF_PV_SIZE + HF_OFF_PV_VOLTAGE, 2, 10, false, false, 1 },
    { TYPE_DC, CH0, FLD_IDC, UNIT_A,   HF_OFF_PV_BASE + 0 * HF_PV_SIZE + HF_OFF_PV_CURRENT, 2, 100, false, false, 2 },
    { TYPE_DC, CH0, FLD_PDC, UNIT_W,   HF_OFF_PV_BASE + 0 * HF_PV_SIZE + HF_OFF_PV_POWER,   2, 10, false, false, 1 },
    { TYPE_DC, CH0, FLD_YD,  UNIT_WH,  HF_OFF_PV_BASE + 0 * HF_PV_SIZE + HF_OFF_PV_ENERGY_DAY, 4, 1, false, false, 0 },
    { TYPE_DC, CH0, FLD_YT,  UNIT_KWH, HF_OFF_PV_BASE + 0 * HF_PV_SIZE + HF_OFF_PV_ENERGY_TOT, 4, 1000, false, false, 3 },
    { TYPE_DC, CH0, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH0, CMD_CALC, false, false, 3 },

    // DC Channel 1 (PV port 1)
    { TYPE_DC, CH1, FLD_UDC, UNIT_V,   HF_OFF_PV_BASE + 1 * HF_PV_SIZE + HF_OFF_PV_VOLTAGE, 2, 10, false, false, 1 },
    { TYPE_DC, CH1, FLD_IDC, UNIT_A,   HF_OFF_PV_BASE + 1 * HF_PV_SIZE + HF_OFF_PV_CURRENT, 2, 100, false, false, 2 },
    { TYPE_DC, CH1, FLD_PDC, UNIT_W,   HF_OFF_PV_BASE + 1 * HF_PV_SIZE + HF_OFF_PV_POWER,   2, 10, false, false, 1 },
    { TYPE_DC, CH1, FLD_YD,  UNIT_WH,  HF_OFF_PV_BASE + 1 * HF_PV_SIZE + HF_OFF_PV_ENERGY_DAY, 4, 1, false, false, 0 },
    { TYPE_DC, CH1, FLD_YT,  UNIT_KWH, HF_OFF_PV_BASE + 1 * HF_PV_SIZE + HF_OFF_PV_ENERGY_TOT, 4, 1000, false, false, 3 },
    { TYPE_DC, CH1, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH1, CMD_CALC, false, false, 3 },

    // DC Channel 2 (PV port 2)
    { TYPE_DC, CH2, FLD_UDC, UNIT_V,   HF_OFF_PV_BASE + 2 * HF_PV_SIZE + HF_OFF_PV_VOLTAGE, 2, 10, false, false, 1 },
    { TYPE_DC, CH2, FLD_IDC, UNIT_A,   HF_OFF_PV_BASE + 2 * HF_PV_SIZE + HF_OFF_PV_CURRENT, 2, 100, false, false, 2 },
    { TYPE_DC, CH2, FLD_PDC, UNIT_W,   HF_OFF_PV_BASE + 2 * HF_PV_SIZE + HF_OFF_PV_POWER,   2, 10, false, false, 1 },
    { TYPE_DC, CH2, FLD_YD,  UNIT_WH,  HF_OFF_PV_BASE + 2 * HF_PV_SIZE + HF_OFF_PV_ENERGY_DAY, 4, 1, false, false, 0 },
    { TYPE_DC, CH2, FLD_YT,  UNIT_KWH, HF_OFF_PV_BASE + 2 * HF_PV_SIZE + HF_OFF_PV_ENERGY_TOT, 4, 1000, false, false, 3 },
    { TYPE_DC, CH2, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH2, CMD_CALC, false, false, 3 },

    // DC Channel 3 (PV port 3)
    { TYPE_DC, CH3, FLD_UDC, UNIT_V,   HF_OFF_PV_BASE + 3 * HF_PV_SIZE + HF_OFF_PV_VOLTAGE, 2, 10, false, false, 1 },
    { TYPE_DC, CH3, FLD_IDC, UNIT_A,   HF_OFF_PV_BASE + 3 * HF_PV_SIZE + HF_OFF_PV_CURRENT, 2, 100, false, false, 2 },
    { TYPE_DC, CH3, FLD_PDC, UNIT_W,   HF_OFF_PV_BASE + 3 * HF_PV_SIZE + HF_OFF_PV_POWER,   2, 10, false, false, 1 },
    { TYPE_DC, CH3, FLD_YD,  UNIT_WH,  HF_OFF_PV_BASE + 3 * HF_PV_SIZE + HF_OFF_PV_ENERGY_DAY, 4, 1, false, false, 0 },
    { TYPE_DC, CH3, FLD_YT,  UNIT_KWH, HF_OFF_PV_BASE + 3 * HF_PV_SIZE + HF_OFF_PV_ENERGY_TOT, 4, 1000, false, false, 3 },
    { TYPE_DC, CH3, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH3, CMD_CALC, false, false, 3 },

    // AC Channel
    { TYPE_AC, CH0, FLD_UAC, UNIT_V,   HF_OFF_AC_VOLTAGE, 2, 10, false, false, 1 },
    { TYPE_AC, CH0, FLD_IAC, UNIT_A,   HF_OFF_AC_CURRENT, 2, 100, false, false, 2 },
    { TYPE_AC, CH0, FLD_PAC, UNIT_W,   HF_OFF_AC_POWER, 2, 10, false, false, 1 },
    { TYPE_AC, CH0, FLD_Q,   UNIT_VAR, HF_OFF_REACTIVE_PWR, 2, 10, true, false, 1 },
    { TYPE_AC, CH0, FLD_F,   UNIT_HZ,  HF_OFF_FREQUENCY, 2, 100, false, false, 2 },
    { TYPE_AC, CH0, FLD_PF,  UNIT_NONE, HF_OFF_POWER_FACTOR, 2, 1000, true, false, 3 },
    { TYPE_AC, CH0, FLD_YD,  UNIT_WH,  HF_OFF_AC_ENERGY_DAY, 4, 1, false, false, 0 },
    { TYPE_AC, CH0, FLD_YT,  UNIT_KWH, HF_OFF_AC_ENERGY_TOT, 4, 1000, false, false, 3 },

    // Inverter totals
    { TYPE_INV, CH0, FLD_T,   UNIT_C,   HF_OFF_TEMPERATURE, 2, 10, true, false, 1 },
    { TYPE_INV, CH0, FLD_YD,  UNIT_WH,  CALC_TOTAL_YD, 0, CMD_CALC, false, true, 0 },
    { TYPE_INV, CH0, FLD_YT,  UNIT_KWH, CALC_TOTAL_YT, 0, CMD_CALC, false, true, 3 },
    { TYPE_INV, CH0, FLD_PDC, UNIT_W,   CALC_TOTAL_PDC, 0, CMD_CALC, false, false, 1 },
    { TYPE_INV, CH0, FLD_EFF, UNIT_PCT, CALC_TOTAL_EFF, 0, CMD_CALC, false, false, 3 },
};

HF_4CH::HF_4CH(uint64_t serial)
    : HiFlowInverter(serial)
{
    _devInfoParser->setHardwareModel("HMS-1600-4WB");
    _devInfoParser->setMaxPower(1600);
    _statisticsParser->setByteAssignment(byteAssignment_HF_4CH,
                                         sizeof(byteAssignment_HF_4CH) / sizeof(byteAssignment_HF_4CH[0]));
}

bool HF_4CH::isValidSerial(uint64_t serial)
{
    uint16_t preSerial = (serial >> 32) & 0xFFFF;
    return preSerial == 0x1164;
}

String HF_4CH::typeName() const
{
    return "HMS-1600-4WB";
}
