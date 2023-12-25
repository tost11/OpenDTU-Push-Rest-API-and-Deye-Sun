#include "DS_2CH.h"

const std::vector<RegisterMapping> registersToRead = {
        RegisterMapping("0010",1,44),//init rated power
        RegisterMapping("AT+YZVER",0,0),//firmware version
        RegisterMapping("0028",1,42),//limit always check
        RegisterMapping("006D",1,2),
        RegisterMapping("006F",1,6),
        RegisterMapping("006E",1,4),
        RegisterMapping("0070",1,8),
        RegisterMapping("003C",1,36),
        RegisterMapping("0041",1,14),
        RegisterMapping("0042",1,16),
        RegisterMapping("003F",2,38),
        RegisterMapping("0045",1,10),
        RegisterMapping("0047",1,12),
        RegisterMapping("0049",1,18),
        RegisterMapping("004C",1,28),
        RegisterMapping("004F",1,20),
        RegisterMapping("003B",1,34),
        RegisterMapping("0056",2,22),
        RegisterMapping("005A",1,32),
        RegisterMapping("0032",1,30),
};

static const byteAssign_t byteAssignment[] = {
        //type, channel, field,  uint, first, byte in buffer, number of bytes in buffer, divisor, isSigned; // allow negative numbers, digits; // number of valid digits after the decimal point
        { TYPE_DC, CH0, FLD_UDC, UNIT_V, 2, 2, 10, false, 1 },
        { TYPE_DC, CH0, FLD_IDC, UNIT_A, 4, 2, 10, false, 2 },
        //{ TYPE_DC, CH0, FLD_PDC, UNIT_W, 6, 2, 10, false, 1 },
        { TYPE_DC, CH0, FLD_PDC, UNIT_W, CALC_PDC, CH0, CMD_CALC, false, 2 },
        { TYPE_DC, CH0, FLD_YD, UNIT_KWH, 14, 2, 100, false, 1 },
        { TYPE_DC, CH0, FLD_YT, UNIT_KWH, 10, 2, 10, false, 0 },
        { TYPE_DC, CH0, FLD_IRR, UNIT_PCT, CALC_IRR_CH, CH0, CMD_CALC, false, 3 },

        { TYPE_DC, CH1, FLD_UDC, UNIT_V, 6, 2, 10, false, 1 },
        { TYPE_DC, CH1, FLD_IDC, UNIT_A, 8, 2, 10, false, 2 },
        //{ TYPE_DC, CH1, FLD_PDC, UNIT_W, 12, 2, 10, false, 1 },
        { TYPE_DC, CH1, FLD_PDC, UNIT_W, CALC_PDC, CH1, CMD_CALC, false, 2 },
        { TYPE_DC, CH1, FLD_YD, UNIT_KWH, 16, 2, 100, false, 1 },
        { TYPE_DC, CH1, FLD_YT, UNIT_KWH, 12, 2, 10, false, 0 },
        { TYPE_DC, CH1, FLD_IRR, UNIT_PCT, CALC_IRR_CH, CH1, CMD_CALC, false, 3 },

        { TYPE_AC, CH0, FLD_UAC, UNIT_V, 18, 2, 10, false, 1 },
        { TYPE_AC, CH0, FLD_IAC, UNIT_A, 28, 2, 10, false, 2 },
        { TYPE_AC, CH0, FLD_PAC, UNIT_W, 22, 4, 10, false, 1 },
        //{ TYPE_AC, CH0, FLD_Q, UNIT_VAR, 26, 2, 10, false, 1 },
        { TYPE_AC, CH0, FLD_F, UNIT_HZ, 20, 2, 100, false, 2 },
        { TYPE_AC, CH0, FLD_PF, UNIT_NONE, 30, 2, 1000, false, 3 },

        { TYPE_INV, CH0, FLD_T, UNIT_C, 32, 2, 100, true, 1 },
        { TYPE_INV, CH0, FLD_EVT_LOG, UNIT_NONE, 34, 2, 1, false, 0 },//current status

        //{ TYPE_AC, CH0, FLD_YD, UNIT_WH, CALC_YD_CH0, 0, CMD_CALC, false, 0 },
        { TYPE_AC, CH0, FLD_YD, UNIT_KWH, 36, 2, 100, false, 1 },
        //{ TYPE_AC, CH0, FLD_YT, UNIT_KWH, CALC_YT_CH0, 0, CMD_CALC, false, 3 },
        { TYPE_AC, CH0, FLD_YT, UNIT_KWH, 38, 4, 10, false, 0 },
        { TYPE_AC, CH0, FLD_PDC, UNIT_W, CALC_PDC_CH0, 0, CMD_CALC, false, 1 },
        { TYPE_AC, CH0, FLD_EFF, UNIT_PCT, CALC_EFF_CH0, 0, CMD_CALC, false, 3 }
};

DS_2CH::DS_2CH(uint64_t serial,const String & model) : DeyeInverter(serial) {
    _devInfoParser->setHardwareModel(model);
    _statisticsParser->setByteAssignment(byteAssignment,sizeof(byteAssignment) / sizeof(byteAssignment[0]));
}

const std::vector<RegisterMapping> &DS_2CH::getRegisteresToRead() {
    return registersToRead;
}
