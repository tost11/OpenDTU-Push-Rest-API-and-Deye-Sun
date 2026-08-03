#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include "BaseInverterHandler.h"
#include "inverters/HiFlowInverter.h"

class HiFlowBLEClass : public BaseInverterHandler<HiFlowInverter, HiFlowStatisticsParser, HiFlowDevInfo, DefaultAlarmLog, HiFlowPowerCommand> {
public:
    HiFlowBLEClass();

    void loop();

    std::shared_ptr<HiFlowInverter> addInverter(const char* name, uint64_t serial, const char* bleMac);
    std::shared_ptr<HiFlowInverter> getInverterByPos(uint8_t pos) override;
    std::shared_ptr<HiFlowInverter> getInverterBySerial(uint64_t serial) override;
    std::shared_ptr<HiFlowInverter> getInverterBySerialString(const String& serial) override;

    void removeInverterBySerial(uint64_t serial) override;
    size_t getNumInverters() const override;

    bool isAllRadioIdle() const override;

    void init() override;

private:
    std::vector<std::shared_ptr<HiFlowInverter>>& _inverters;
    std::mutex _mutex;
};

extern HiFlowBLEClass HiFlowBle;
