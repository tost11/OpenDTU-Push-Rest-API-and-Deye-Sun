#pragma once

#include "CustomModbusDeyeInverter.h"

class CMOD_DS_1CH: public CustomModbusDeyeInverter {

public:
    CMOD_DS_1CH(uint64_t serial,const String & model);
    ~CMOD_DS_1CH() = default;
};
