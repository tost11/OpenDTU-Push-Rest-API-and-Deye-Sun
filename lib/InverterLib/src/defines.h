#pragma once

#include <WString.h>

typedef enum { // ToDo: to be verified by field tests
    AbsolutNonPersistent = 0x0000, // 0
    RelativNonPersistent = 0x0001, // 1
    AbsolutPersistent = 0x0100, // 256
    RelativPersistent = 0x0101 // 257
} PowerLimitControlType;

typedef enum {
    CMD_OK,
    CMD_NOK,
    CMD_PENDING
} LastCommandSuccess;

enum inverter_type {
    Inverter_Hoymiles = 0,
    Inverter_DeyeSun,
    Inverter_HoymilesW,

    Inverter_count
};

static inline inverter_type to_inverter_type(const String & type){
    if(type == "Hoymiles"){
        return inverter_type::Inverter_Hoymiles;
    }
    if(type == "DeyeSun"){
        return inverter_type::Inverter_DeyeSun;
    }
    if(type == "HoymilesW"){
        return inverter_type::Inverter_HoymilesW;
    }
    return inverter_type::Inverter_count;
}

static inline String from_inverter_type(inverter_type type){
    if(type == inverter_type::Inverter_Hoymiles){
        return "Hoymiles";
    }
    if(type == inverter_type::Inverter_DeyeSun){
        return "DeyeSun";
    }
    if(type == inverter_type::Inverter_HoymilesW){
        return "HoymilesW";
    }
    return "";
}