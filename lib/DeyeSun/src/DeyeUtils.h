#pragma once


#include <cstdint>

class DeyeUtils {
public:
    static float defaultParseFloat(uint8_t ptr, uint8_t *buff,uint16_t div = 1);
    static uint32_t defaultParseUInt(uint8_t ptr, uint8_t *buff,uint16_t div = 1);
};
