#include "CMOD_DS_1CH.h"

//todo move positions
static const byteAssign_t byteAssignment[] = {
        //type, channel, field,  uint, first, byte in buffer, number of bytes in buffer, divisor, isSigned; // allow negative numbers, little endian, digits; // number of valid digits after the decimal point
        { TYPE_DC, CH0, FLD_UDC, UNIT_V, 100, 2, 10, false,true, 1 },
        { TYPE_DC, CH0, FLD_IDC, UNIT_A, 102, 2, 10, false,true, 2 },
        { TYPE_DC, CH0, FLD_PDC, UNIT_W, CALC_PDC, CH0, CMD_CALC, false, true, 2 },
        { TYPE_DC, CH0, FLD_YD, UNIT_KWH, 12, 2, 10, false, true, 1 },
        { TYPE_DC, CH0, FLD_YT, UNIT_KWH, 20, 4, 10, false, true, 1 },
        { TYPE_DC, CH0, FLD_IRR, UNIT_PCT, CALC_CH_IRR, CH0, CMD_CALC, false, true, 3 },

        { TYPE_AC, CH0, FLD_UAC, UNIT_V, 28, 2, 10, false,false, 1 },
        { TYPE_AC, CH0, FLD_IAC, UNIT_A, 34, 2, 10, false,false, 2 },
        { TYPE_AC, CH0, FLD_PAC, UNIT_W, CALC_PDC, CH0, CMD_CALC, false, true, 2 },
        //{ TYPE_AC, CH0, FLD_Q, UNIT_VAR, 26, 2, 10, false, 1 },
        { TYPE_AC, CH0, FLD_F, UNIT_HZ, 40, 2, 100, false,true, 2 },
        //{ TYPE_AC, CH0, FLD_PF, UNIT_NONE, 30, 2, 1000, false,true, 3 },
        { TYPE_AC, CH0, FLD_YD, UNIT_KWH, 2, 2, 10, false,true, 1 },
        { TYPE_AC, CH0, FLD_YT, UNIT_KWH, 8, 4, 10, false,true, 1 },

        { TYPE_INV, CH0, FLD_T, UNIT_C, 62, 2, 100, false,true, 1 },
        //{ TYPE_INV, CH0, FLD_EVT_LOG, UNIT_NONE, 34, 2, 1, false,true, 0 },//current status
        { TYPE_INV, CH0, FLD_YD, UNIT_KWH, CALC_TOTAL_YD, 0, CMD_CALC, false,true, 1 },
        { TYPE_INV, CH0, FLD_YT, UNIT_KWH, CALC_TOTAL_YT, 0, CMD_CALC, false,true, 1 },
        { TYPE_INV, CH0, FLD_PDC, UNIT_W, CALC_TOTAL_PDC, 0, CMD_CALC, false,true, 1 },
        //dosnt make sense because red ac value highter then dc value
        //{ TYPE_INV, CH0, FLD_EFF, UNIT_PCT, CALC_TOTAL_EFF, 0, CMD_CALC, false,true, 3 }
};

CMOD_DS_1CH::CMOD_DS_1CH(uint64_t serial,const String & modelType):
CustomModbusDeyeInverter(serial){
    _devInfoParser->setHardwareModel(modelType);
    _statisticsParser->setByteAssignment(byteAssignment,sizeof(byteAssignment) / sizeof(byteAssignment[0]));
}
