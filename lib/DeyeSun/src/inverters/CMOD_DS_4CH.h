#pragma once

#include "CustomModbusDeyeInverter.h"

class CMOD_DS_4CH: public CustomModbusDeyeInverter {

public:
    CMOD_DS_4CH(uint64_t serial,const String & model);
    ~CMOD_DS_4CH() = default;
};
