#pragma once

#include "DeyeInverter.h"

class DS_2CH: public DeyeInverter{
public:
    DS_2CH(uint64_t serial,const String & model);
    ~DS_2CH() = default;

private:
    const std::vector<RegisterMapping> &getRegisteresToRead() override;
};