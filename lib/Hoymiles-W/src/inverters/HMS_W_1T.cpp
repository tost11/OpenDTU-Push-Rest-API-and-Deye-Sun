#include "HMS_W_1T.h"

static const byteAssign_t byteAssignment[] = {
        //type, channel, field,  uint, first, byte in buffer, number of bytes in buffer, divisor, isSigned, rendered digets; // allow negative numbers, digits; // number of valid digits after the decimal point
        { TYPE_DC, CH0, FLD_UDC, UNIT_V, 16, 2, 10, false,false, 1 },
        { TYPE_DC, CH0, FLD_IDC, UNIT_A, 14, 2, 100, false,false, 2 },
        { TYPE_DC, CH0, FLD_PDC, UNIT_W, 18, 2, 10, false,false, 2 },
        { TYPE_DC, CH0, FLD_YD, UNIT_WH, 20, 4, 1, false,false, 0 },
        { TYPE_DC, CH0, FLD_YT, UNIT_KWH, 24, 4, 1000, false,false, 3 },
        { TYPE_DC, CH0, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH0, CMD_CALC, false,false, 3 },

        { TYPE_AC, CH0, FLD_UAC, UNIT_V, 2, 2, 10, false,false, 1 },
        { TYPE_AC, CH0, FLD_IAC, UNIT_A, 0, 2, 100, false,false, 2 },
        { TYPE_AC, CH0, FLD_PAC, UNIT_W, 4, 2, 10, false,false, 1 },
        { TYPE_AC, CH0, FLD_Q, UNIT_VAR, 76, 2, 1000, true,false, 1 },
        { TYPE_AC, CH0, FLD_F, UNIT_HZ, 70, 2, 100, false,false, 2 },
        { TYPE_AC, CH0, FLD_PF, UNIT_NONE, 74, 2, 1, true,false, 3 },
        { TYPE_AC, CH0, FLD_YD, UNIT_WH, 6, 2, 1, false,false, 0 },
        { TYPE_AC, CH0, FLD_YT, UNIT_KWH, 10, 4, 1000, false,false, 3 },

        { TYPE_INV, CH0, FLD_T, UNIT_C, 72, 2, 10, true,false, 1 },
        //{ TYPE_INV, CH0, FLD_EVT_LOG, UNIT_NONE, 34, 4, 1, true, 0 },//current status

        { TYPE_INV, CH0, FLD_YD, UNIT_WH, CALC_TOTAL_YD, 0, CMD_CALC, false,true, 0 },
        { TYPE_INV, CH0, FLD_YT, UNIT_KWH, CALC_TOTAL_YT, 0, CMD_CALC, false,true, 3 },
        { TYPE_INV, CH0, FLD_PDC, UNIT_W, CALC_TOTAL_PDC, 0, CMD_CALC, false,false, 1 },
        { TYPE_INV, CH0, FLD_EFF, UNIT_PCT, CALC_TOTAL_EFF, 0, CMD_CALC, false,false, 3 }
};
HMS_W_1T::HMS_W_1T(uint64_t serial):
HoymilesWInverter(serial){
    _devInfoParser->setHardwareModel(typeName());
    _statisticsParser->setByteAssignment(byteAssignment,sizeof(byteAssignment) / sizeof(byteAssignment[0]));
}

bool HMS_W_1T::isValidSerial(const uint64_t serial)
{
    uint16_t preSerial = (serial >> 32) & 0xffff;
    return
            preSerial == 0x1125 ||
            preSerial == 0x1124 ||
            preSerial == 0x1123 ||
            preSerial == 0x1122 ||

            preSerial == 0x1403
            ;
}

String HMS_W_1T::typeName() const
{
    return "HMS-*W-1T";
}
