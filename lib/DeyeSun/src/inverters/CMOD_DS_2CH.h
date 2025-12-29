#pragma once

#include "CustomModbusDeyeInverter.h"

class CMOD_DS_2CH: public CustomModbusDeyeInverter {

public:
    CMOD_DS_2CH(uint64_t serial,const String & model);
    ~CMOD_DS_2CH() = default;
};
