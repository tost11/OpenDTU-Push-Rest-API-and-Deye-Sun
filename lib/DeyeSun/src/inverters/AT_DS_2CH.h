#pragma once

#include "AtCommandsDeyeInverter.h"

class AT_DS_2CH: public AtCommandsDeyeInverter{
public:
    AT_DS_2CH(uint64_t serial,const String & model);
    ~AT_DS_2CH() = default;

private:
    const std::vector<RegisterMapping> &getRegisteresToRead() override;
};