#pragma once

#include "HoymilesWInverter.h"

class HMS_W_2T: public HoymilesWInverter{
public:
    HMS_W_2T(uint64_t serial,const String & model,Print & print);
    ~HMS_W_2T() = default;

private:
};