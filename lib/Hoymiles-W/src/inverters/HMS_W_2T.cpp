#include "HMS_W_2T.h"

/*static const byteAssign_t byteAssignment[] = {
        //type, channel, field,  uint, first, byte in buffer, number of bytes in buffer, divisor, isSigned, rendered digets; // allow negative numbers, digits; // number of valid digits after the decimal point
        { TYPE_DC, CH0, FLD_UDC, UNIT_V, 76, 4, 10, true,false, 1 },
        { TYPE_DC, CH0, FLD_IDC, UNIT_A, 80, 4, 100, true,false, 2 },
        { TYPE_DC, CH0, FLD_PDC, UNIT_W, 84, 4, 10, true,false, 2 },
        { TYPE_DC, CH0, FLD_YD, UNIT_KWH, 92, 4, 1000, true,false, 1 },
        { TYPE_DC, CH0, FLD_YT, UNIT_KWH, 88, 4, 1000, true,false, 0 },
        { TYPE_DC, CH0, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH0, CMD_CALC, true,false, 3 },

        { TYPE_DC, CH1, FLD_UDC, UNIT_V, 116, 4, 10, true,false, 1 },
        { TYPE_DC, CH1, FLD_IDC, UNIT_A, 120, 4, 100, true,false, 2 },
        { TYPE_DC, CH1, FLD_PDC, UNIT_W, 124, 4, 10, true,false, 2 },
        { TYPE_DC, CH1, FLD_YD, UNIT_KWH, 132, 4, 1000, true,false, 1 },
        { TYPE_DC, CH1, FLD_YT, UNIT_KWH, 128, 4, 1000, true,false, 0 },
        { TYPE_DC, CH1, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH0, CMD_CALC, true,false, 3 },

        { TYPE_AC, CH0, FLD_UAC, UNIT_V, 12, 4, 10, true,false, 1 },
        { TYPE_AC, CH0, FLD_IAC, UNIT_A, 28, 4, 100, true,false, 2 },
        { TYPE_AC, CH0, FLD_PAC, UNIT_W, 20, 4, 10, true,false, 1 },
        { TYPE_AC, CH0, FLD_Q, UNIT_VAR, 24, 4, 1, true,false, 1 },
        { TYPE_AC, CH0, FLD_F, UNIT_HZ, 16, 4, 100, true,false, 2 },
        { TYPE_AC, CH0, FLD_PF, UNIT_NONE, 32, 4, 1000, true,false, 3 },

        { TYPE_INV, CH0, FLD_T, UNIT_C, 36, 4, 10, true,false, 1 },
        //{ TYPE_INV, CH0, FLD_EVT_LOG, UNIT_NONE, 34, 4, 1, true, 0 },//current status

        { TYPE_INV, CH0, FLD_YD, UNIT_WH, CALC_TOTAL_YD, 0, CMD_CALC, true,false, 0 },
        { TYPE_INV, CH0, FLD_YT, UNIT_KWH, CALC_TOTAL_YT, 0, CMD_CALC, true,false, 3 },
        { TYPE_INV, CH0, FLD_PDC, UNIT_W, CALC_TOTAL_PDC, 0, CMD_CALC, true,false, 1 },
        { TYPE_INV, CH0, FLD_EFF, UNIT_PCT, CALC_TOTAL_EFF, 0, CMD_CALC, true,false, 3 }
};*/

static const byteAssign_t byteAssignment[] = {
        //type, channel, field,  uint, first, byte in buffer, number of bytes in buffer, divisor, isSigned, rendered digets; // allow negative numbers, digits; // number of valid digits after the decimal point
        { TYPE_DC, CH0, FLD_UDC, UNIT_V, 16, 2, 10, false,false, 1 },
        { TYPE_DC, CH0, FLD_IDC, UNIT_A, 14, 2, 100, false,false, 2 },
        { TYPE_DC, CH0, FLD_PDC, UNIT_W, 18, 2, 10, false,false, 2 },
        { TYPE_DC, CH0, FLD_YD, UNIT_KWH, 20, 4, 1000, false,false, 2 },
        { TYPE_DC, CH0, FLD_YT, UNIT_KWH, 24, 4, 1000, false,false, 1 },
        { TYPE_DC, CH0, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH0, CMD_CALC, false,false, 3 },

        { TYPE_DC, CH1, FLD_UDC, UNIT_V, 30, 2, 10, false,false, 1 },
        { TYPE_DC, CH1, FLD_IDC, UNIT_A, 28, 2, 100, false,false, 2 },
        { TYPE_DC, CH1, FLD_PDC, UNIT_W, 32, 2, 10, false,false, 2 },
        { TYPE_DC, CH1, FLD_YD, UNIT_KWH, 34, 4, 1000, false,false, 2 },
        { TYPE_DC, CH1, FLD_YT, UNIT_KWH, 38, 4, 1000, false,false, 1 },
        { TYPE_DC, CH1, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH0, CMD_CALC, false,false, 3 },

        { TYPE_AC, CH0, FLD_UAC, UNIT_V, 2, 2, 10, false,false, 1 },
        { TYPE_AC, CH0, FLD_IAC, UNIT_A, 0, 2, 100, false,false, 2 },
        { TYPE_AC, CH0, FLD_PAC, UNIT_W, 4, 2, 10, false,false, 1 },
        /*{ TYPE_AC, CH0, FLD_Q, UNIT_VAR, 24, 4, 1, true,false, 1 },*/
        { TYPE_AC, CH0, FLD_F, UNIT_HZ, 56, 2, 100, true,false, 2 },
        /*{ TYPE_AC, CH0, FLD_PF, UNIT_NONE, 32, 4, 1000, true,false, 3 },*/
        //TODO use total values

        { TYPE_INV, CH0, FLD_T, UNIT_C, 58, 4, 10, false,false, 1 },
        //{ TYPE_INV, CH0, FLD_EVT_LOG, UNIT_NONE, 34, 4, 1, true, 0 },//current status

        { TYPE_INV, CH0, FLD_YD, UNIT_WH, CALC_TOTAL_YD, 0, CMD_CALC, false,false, 0 },
        { TYPE_INV, CH0, FLD_YT, UNIT_KWH, CALC_TOTAL_YT, 0, CMD_CALC, false,false, 3 },
        { TYPE_INV, CH0, FLD_PDC, UNIT_W, CALC_TOTAL_PDC, 0, CMD_CALC, false,false, 1 },
        { TYPE_INV, CH0, FLD_EFF, UNIT_PCT, CALC_TOTAL_EFF, 0, CMD_CALC, false,false, 3 }
};

HMS_W_2T::HMS_W_2T(uint64_t serial,const String & model,Print & print) :
HoymilesWInverter(serial,print) {
    _devInfoParser->setHardwareModel(model);
    _statisticsParser->setByteAssignment(byteAssignment,sizeof(byteAssignment) / sizeof(byteAssignment[0]));
}

