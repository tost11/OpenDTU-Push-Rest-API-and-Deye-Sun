#pragma once

#include "HiFlowInverter.h"

/**
 * HiFlow Pro 2-channel (HMS-800-2WB, HF-800-WB)
 * 2 MPPT inputs, single-phase AC output.
 */
class HF_2CH : public HiFlowInverter {
public:
    HF_2CH(uint64_t serial);
    ~HF_2CH() = default;

    static bool isValidSerial(uint64_t serial);
    String typeName() const override;
};
