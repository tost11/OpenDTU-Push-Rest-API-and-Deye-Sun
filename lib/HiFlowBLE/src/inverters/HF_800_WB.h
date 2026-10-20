#pragma once

#include "HiFlowInverter.h"

/**
 * HF-800-WB - single 100V string inverter that the device mis-reports as two
 * PV ports. The two reported ports are merged into one (voltage/power/yields
 * summed, DC current averaged). Known output power 800W.
 */
class HF_800_WB : public HiFlowInverter {
public:
    HF_800_WB(uint64_t serial);
    ~HF_800_WB() = default;

    static bool isValidSerial(uint64_t serial);
    String typeName() const override;

protected:
    void adjustPvData(HiFlowData &hfData) override;
};
