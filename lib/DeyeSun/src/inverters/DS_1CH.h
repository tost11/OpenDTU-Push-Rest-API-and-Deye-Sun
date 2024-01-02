#pragma once

#include "DeyeInverter.h"

class DS_1CH: public DeyeInverter {

public:
    DS_1CH(uint64_t serial,const String & model,Print & print);
    ~DS_1CH() = default;

private:
    const std::vector<RegisterMapping> & getRegisteresToRead() override;
};
