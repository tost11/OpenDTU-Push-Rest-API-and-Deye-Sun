#pragma once

#include <WString.h>

enum CountryModeId_t {
    MODE_EU,
    MODE_US,
    MODE_BR,
    CountryModeId_Max
};

typedef enum {
    AbsolutNonPersistent,
    RelativNonPersistent,
    AbsolutPersistent,
    RelativPersistent,
    PowerLimitControl_Max
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
    Inverter_HiFlowBLE,

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
    if(type == "HiFlowBLE"){
        return inverter_type::Inverter_HiFlowBLE;
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
    if(type == inverter_type::Inverter_HiFlowBLE){
        return "HiFlowBLE";
    }
    return "";
}

static inline uint16_t getDefaultPollTimeForInverterType(inverter_type type){
    if(type == inverter_type::Inverter_HoymilesW){
        //on low inverter firmware version 1.*.* it seems fetching is only allowed ever 30sec if not it result in hanging values
        return 31;
    }
    if(type == inverter_type::Inverter_DeyeSun){
        return 20;
    }
    if(type == inverter_type::Inverter_HiFlowBLE){
        return 15;
    }
    return 0;
}