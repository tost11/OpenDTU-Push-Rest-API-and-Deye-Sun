#pragma once

#include <cstdint>
#include <type_traits>
#include <WString.h>

class DeyeUtils {
public:
    static float defaultParseFloat(uint8_t ptr,const uint8_t *buff,uint16_t div = 1);
    static uint32_t defaultParseUInt(uint8_t ptr,const uint8_t *buff,uint16_t div = 1);


    template <typename T,typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
    static String lengthToString( T length, int fill)
    {
        char res[fill+1];

        auto formater = String("%0") + String(fill).c_str() + "u";
        snprintf(res,fill+1,formater.c_str(),length);

        return String(res);
    }

    template <typename T,typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
    static String lengthToHexString( T length, int fill)
    {
        char res[fill+1];

        auto formater = String("%0") + String(fill).c_str() + "x";
        snprintf(res,fill+1,formater.c_str(),length);

        return String(res);
    }
};
