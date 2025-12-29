#include "DeyeUtils.h"

float DeyeUtils::defaultParseFloat(uint8_t ptr,const uint8_t *buff,uint16_t div) {
    uint8_t end = ptr + 2;
    uint32_t val = 0;
    do {
        val <<= 8;
        val |= buff[ptr];
    } while (++ptr != end);

    return static_cast<float>(val) / div;
}

uint32_t DeyeUtils::defaultParseUInt(uint8_t ptr, const uint8_t *buff,uint16_t div){
    uint8_t end = ptr + 2;
    uint32_t val = 0;
    do {
        val <<= 8;
        val |= buff[ptr];
    } while (++ptr != end);

    return val / div;
}
