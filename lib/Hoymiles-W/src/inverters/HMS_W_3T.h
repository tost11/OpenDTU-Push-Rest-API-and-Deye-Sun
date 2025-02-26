#pragma once

#include "HoymilesWInverter.h"

class HMS_W_3T: public HoymilesWInverter {

public:
    HMS_W_3T(uint64_t serial);
    ~HMS_W_3T() = default;
    static bool isValidSerial(const uint64_t serial);
    String typeName() const;
};
