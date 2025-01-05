#pragma once

#include "HoymilesWInverter.h"

class HMS_W_1T: public HoymilesWInverter {

public:
    HMS_W_1T(uint64_t serial,const String & model,Print & print);
    ~HMS_W_1T() = default;

private:
};
