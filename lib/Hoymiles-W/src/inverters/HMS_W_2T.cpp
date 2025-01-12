#include "HMS_W_2T.h"

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

        { TYPE_AC, CH0, FLD_UAC, UNIT_V, 2, 2, 10, false,false, 1 },
        { TYPE_AC, CH0, FLD_IAC, UNIT_A, 0, 2, 100, false,false, 2 },
        { TYPE_AC, CH0, FLD_PAC, UNIT_W, 4, 2, 10, false,false, 1 },
        { TYPE_AC, CH0, FLD_Q, UNIT_VAR, 76, 2, 1000, true,false, 1 },
        { TYPE_AC, CH0, FLD_F, UNIT_HZ, 70, 2, 100, false,false, 2 },
        { TYPE_AC, CH0, FLD_PF, UNIT_NONE, 74, 2, 1, true,false, 3 },

        { TYPE_INV, CH0, FLD_T, UNIT_C, 72, 4, 10, true,false, 1 },
        //{ TYPE_INV, CH0, FLD_EVT_LOG, UNIT_NONE, 34, 4, 1, true, 0 },//current status

        { TYPE_INV, CH0, FLD_YD, UNIT_WH, 6, 2, 1, false,false, 0 },
        { TYPE_INV, CH0, FLD_YT, UNIT_KWH, 10, 2, 1000, false,false, 3 },
        { TYPE_INV, CH0, FLD_PDC, UNIT_W, CALC_TOTAL_PDC, 0, CMD_CALC, false,false, 1 },
        { TYPE_INV, CH0, FLD_EFF, UNIT_PCT, CALC_TOTAL_EFF, 0, CMD_CALC, false,false, 3 }
};

HMS_W_2T::HMS_W_2T(uint64_t serial,Print & print) :
HoymilesWInverter(serial,print) {
    _devInfoParser->setHardwareModel(typeName());
    _statisticsParser->setByteAssignment(byteAssignment,sizeof(byteAssignment) / sizeof(byteAssignment[0]));
    Serial.printf("Serial string is: %s",_serialString.c_str());
    if(_serialString.startsWith("1412")){
        _devInfoParser->setMaxPower(800);//TODO find other serials
    }
}

bool HMS_W_2T::isValidSerial(const uint64_t serial)
{
    // serial >= 0x141200000000 && serial <= 0x1412ffffffff
    
    //TODO check if this here is correct
    uint8_t preId[2];
    preId[0] = (uint8_t)(serial >> 40);
    preId[1] = (uint8_t)(serial >> 32);

    if ((uint8_t)(((((uint16_t)preId[0] << 8) | preId[1]) >> 4) & 0xff) == 0x41) {
        return true;
    }

    if ((((preId[1] & 0xf0) == 0x00) || ((preId[1] & 0xf0) == 0x10))
        && (((preId[0] == 0x20) && (preId[1] == 0x13)) || ((preId[0] == 0x21) && (preId[1] == 0x12)))) {
        return true;
    }

    return false;
}

String HMS_W_2T::typeName() const
{
    return "HMS-*W-2T";
}

