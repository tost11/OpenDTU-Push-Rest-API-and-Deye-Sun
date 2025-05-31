#include "HMS_W_4T.h"

static const byteAssign_t byteAssignment[] = {
        //type, channel, field,  uint, first, byte in buffer, number of bytes in buffer, divisor, isSigned, rendered digets; // allow negative numbers, digits; // number of valid digits after the decimal point
        { TYPE_DC, CH0, FLD_UDC, UNIT_V, 16, 2, 10, false,false, 1 },
        { TYPE_DC, CH0, FLD_IDC, UNIT_A, 14, 2, 100, false,false, 2 },
        { TYPE_DC, CH0, FLD_PDC, UNIT_W, 18, 2, 10, false,false, 2 },
        { TYPE_DC, CH0, FLD_YD, UNIT_WH, 20, 4, 1, false,false, 0 },
        { TYPE_DC, CH0, FLD_YT, UNIT_KWH, 24, 4, 1000, false,false, 3 },
        { TYPE_DC, CH0, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH0, CMD_CALC, false,false, 3 },

        { TYPE_DC, CH1, FLD_UDC, UNIT_V, 30, 2, 10, false,false, 1 },
        { TYPE_DC, CH1, FLD_IDC, UNIT_A, 28, 2, 100, false,false, 2 },
        { TYPE_DC, CH1, FLD_PDC, UNIT_W, 32, 2, 10, false,false, 2 },
        { TYPE_DC, CH1, FLD_YD, UNIT_WH, 34, 4, 1, false,false, 0 },
        { TYPE_DC, CH1, FLD_YT, UNIT_KWH, 38, 4, 1000, false,false, 3 },
        { TYPE_DC, CH1, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH0, CMD_CALC, false,false, 3 },

        { TYPE_DC, CH2, FLD_UDC, UNIT_V, 44, 2, 10, false,false, 1 },
        { TYPE_DC, CH2, FLD_IDC, UNIT_A, 42, 2, 100, false,false, 2 },
        { TYPE_DC, CH2, FLD_PDC, UNIT_W, 46, 2, 10, false,false, 2 },
        { TYPE_DC, CH2, FLD_YD, UNIT_WH, 48, 4, 1, false,false, 0 },
        { TYPE_DC, CH2, FLD_YT, UNIT_KWH, 50, 4, 1000, false,false, 3 },
        { TYPE_DC, CH2, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH0, CMD_CALC, false,false, 3 },

        { TYPE_DC, CH3, FLD_UDC, UNIT_V, 56, 2, 10, false,false, 1 },
        { TYPE_DC, CH3, FLD_IDC, UNIT_A, 54, 2, 100, false,false, 2 },
        { TYPE_DC, CH3, FLD_PDC, UNIT_W, 58, 2, 10, false,false, 2 },
        { TYPE_DC, CH3, FLD_YD, UNIT_WH, 62, 4, 1, false,false, 0 },
        { TYPE_DC, CH3, FLD_YT, UNIT_KWH, 66, 4, 1000, false,false, 3 },
        { TYPE_DC, CH3, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH0, CMD_CALC, false,false, 3 },

        { TYPE_AC, CH0, FLD_UAC, UNIT_V, 2, 2, 10, false,false, 1 },
        { TYPE_AC, CH0, FLD_IAC, UNIT_A, 0, 2, 100, false,false, 2 },
        { TYPE_AC, CH0, FLD_PAC, UNIT_W, 4, 2, 10, false,false, 1 },
        { TYPE_AC, CH0, FLD_Q, UNIT_VAR, 76, 2, 1000, true,false, 1 },
        { TYPE_AC, CH0, FLD_F, UNIT_HZ, 70, 2, 100, false,false, 2 },
        { TYPE_AC, CH0, FLD_PF, UNIT_NONE, 74, 2, 1, true,false, 3 },
        { TYPE_AC, CH0, FLD_YD, UNIT_WH, 6, 4, 1, false,false, 0 },
        { TYPE_AC, CH0, FLD_YT, UNIT_KWH, 10, 4, 1000, false,false, 3 },

        { TYPE_INV, CH0, FLD_T, UNIT_C, 72, 2, 10, true,false, 1 },
        //{ TYPE_INV, CH0, FLD_EVT_LOG, UNIT_NONE, 34, 4, 1, true, 0 },//current status
        { TYPE_INV, CH0, FLD_YD, UNIT_WH, CALC_TOTAL_YD, 0, CMD_CALC, false,true, 0 },
        { TYPE_INV, CH0, FLD_YT, UNIT_KWH, CALC_TOTAL_YT, 0, CMD_CALC, false,true, 3 },
        { TYPE_INV, CH0, FLD_PDC, UNIT_W, CALC_TOTAL_PDC, 0, CMD_CALC, false,false, 1 },
        { TYPE_INV, CH0, FLD_EFF, UNIT_PCT, CALC_TOTAL_EFF, 0, CMD_CALC, false,false, 3 }
};

HMS_W_4T::HMS_W_4T(uint64_t serial):
HoymilesWInverter(serial){
    _devInfoParser->setHardwareModel(typeName());
    _statisticsParser->setByteAssignment(byteAssignment,sizeof(byteAssignment) / sizeof(byteAssignment[0]));
}

bool HMS_W_4T::isValidSerial(const uint64_t serial)
{
    uint16_t preSerial = (serial >> 32) & 0xffff;
    return
            preSerial == 0x1164 ||
            preSerial == 0x1162 ||
            preSerial == 0x1151 ||

            preSerial == 0x1222
            ;
}

String HMS_W_4T::typeName() const
{
    return "HMS-*W-4T";
}
