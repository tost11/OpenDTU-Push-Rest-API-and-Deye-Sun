#pragma once

#include "HiFlowInverter.h"

/**
 * HiFlow Pro 1-channel (HMS-800-1WB)
 * 1 MPPT input, single-phase AC output.
 * Serial prefix not yet known - isValidSerial is stubbed (returns false) until set.
 */
class HF_1CH : public HiFlowInverter {
public:
    HF_1CH(uint64_t serial);
    ~HF_1CH() = default;

    static bool isValidSerial(uint64_t serial);
    String typeName() const override;
};
