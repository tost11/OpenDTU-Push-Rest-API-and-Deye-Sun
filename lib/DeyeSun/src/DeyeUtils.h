#pragma once

#include <cstdint>
#include <type_traits>
#include <WString.h>
#include <cstdio>
#include <string>

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
    static std::string lengthToHexString( T length, int fill)
    {
        char res[fill+1];

        auto formater = String("%0") + String(fill).c_str() + "x";
        snprintf(res,fill+1,formater.c_str(),length);
        
        return std::string(res);
    }

    static unsigned short modbusCRC16FromHex(const std::string &message){
        const unsigned short generator = 0xA001;
        unsigned short crc = 0xFFFF;
        for(int i = 0; i < message.length(); ++i)
        {
            crc ^= (unsigned short)message[i];
            for(int b = 0; b < 8; ++b)
            {
                if((crc & 1) != 0)
                {
                    crc >>= 1;
                    crc ^= generator;
                }
                else
                    crc >>= 1;

            }
        }
        return crc;
    }

    static unsigned short modbusCRC16FromHex(const String &message){
        const unsigned short generator = 0xA001;
        unsigned short crc = 0xFFFF;
        for(int i = 0; i < message.length(); ++i)
        {
            crc ^= (unsigned short)message[i];
            for(int b = 0; b < 8; ++b)
            {
                if((crc & 1) != 0)
                {
                    crc >>= 1;
                    crc ^= generator;
                }
                else
                    crc >>= 1;

            }
        }
        return crc;
    }

    static String modbusCRC16FromASCII(const String & input) {
        //Serial.print("Calculating crc for: ");
        //Serial.println(input);

        String hexString;

        for(int i=0;i<input.length();i=i+2){
            unsigned number = hex_char_to_int( input[ i ] ); // most signifcnt nibble
            unsigned lsn = hex_char_to_int( input[ i + 1 ] ); // least signt nibble
            number = (number << 4) + lsn;
            hexString += (char)number;
        }

        unsigned short res = modbusCRC16FromHex(hexString);
        std::string stringRes = DeyeUtils::lengthToHexString(res,4);

        return String()+stringRes[2]+stringRes[3]+stringRes[0]+stringRes[1];
    }

    static std::string modbusCRC16FromASCII(const std::string & input) {

        //Serial.print("Calculating crc for: ");
        //Serial.println(input);

        std::string hexString;

        for(int i=0;i<input.length();i=i+2){
            unsigned number = hex_char_to_int( input[ i ] ); // most signifcnt nibble
            unsigned lsn = hex_char_to_int( input[ i + 1 ] ); // least signt nibble
            number = (number << 4) + lsn;
            hexString += (char)number;
        }

        unsigned short res = modbusCRC16FromHex(hexString);
        std::string stringRes = DeyeUtils::lengthToHexString(res,4);

        return std::string()+stringRes[2]+stringRes[3]+stringRes[0]+stringRes[1];
    }

    static unsigned hex_char_to_int( char c ) {
        unsigned result = -1;
        if( ('0' <= c) && (c <= '9') ) {
            result = c - '0';
        }
        else if( ('A' <= c) && (c <= 'F') ) {
            result = 10 + c - 'A';
        }
        else if( ('a' <= c) && (c <= 'f') ) {
            result = 10 + c - 'a';
        }
        else {
            assert( 0 );
        }
        return result;
    }

    static std::string hex_to_bytes(const std::string_view &hex) {
        std::size_t i = 0, size = hex.size();
        if (size >= 2 && hex[0] == '0' && hex[1] == 'x')
            i += 2;

        std::string result;
        for (; i + 1 < size; i += 2) {
            char octet_chars[] = { hex[i], hex[i + 1], '\0' };

            char *end;
            unsigned long octet = std::strtoul(octet_chars, &end, 16);
            if (end != octet_chars + 2) {
                assert(0);
            }

            result.push_back(static_cast<char>(octet));
        }

        if (i != size) {
            assert(0);
        }

        return result;
    }
};
