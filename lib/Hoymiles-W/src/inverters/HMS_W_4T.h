#pragma once

#include "HoymilesWInverter.h"

class HMS_W_4T: public HoymilesWInverter {

public:
    HMS_W_4T(uint64_t serial,Print & print);
    ~HMS_W_4T() = default;
    static bool isValidSerial(const uint64_t serial);
    String typeName() const;
};
