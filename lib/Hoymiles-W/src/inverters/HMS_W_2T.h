#pragma once

#include "HoymilesWInverter.h"

class HMS_W_2T: public HoymilesWInverter{
public:
    HMS_W_2T(uint64_t serial,Print & print);
    ~HMS_W_2T() = default;
    static bool isValidSerial(const uint64_t serial);
    String typeName() const;
};