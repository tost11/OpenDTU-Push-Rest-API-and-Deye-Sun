#pragma once

#include "HiFlowInverter.h"

/**
 * HiFlow Pro 4-channel (HMS-1600-4WB)
 * 4 MPPT inputs, single-phase AC output.
 * Also used as default fallback for unknown serial prefixes.
 */
class HF_4CH : public HiFlowInverter {
public:
    HF_4CH(uint64_t serial);
    ~HF_4CH() = default;

    static bool isValidSerial(uint64_t serial);
    String typeName() const override;
};
