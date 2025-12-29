#pragma once

#include "AtCommandsDeyeInverter.h"

class AT_DS_1CH: public AtCommandsDeyeInverter {

public:
    AT_DS_1CH(uint64_t serial,const String & model);
    ~AT_DS_1CH() = default;

private:
    const std::vector<RegisterMapping> & getRegisteresToRead() override;
};
