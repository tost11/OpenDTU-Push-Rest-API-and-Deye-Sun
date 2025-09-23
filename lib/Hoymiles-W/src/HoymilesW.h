#pragma once

#include <Print.h>
#include <memory>
#include <vector>
#include <mutex>
#include <HardwareSerial.h>
#include "BaseInverterHandler.h"
#include "inverters/HoymilesWInverter.h"

class HoymilesWClass: public BaseInverterHandler<HoymilesWInverter,HoymilesWStatisticsParser,HoymilesWDevInfo,HoymilesWAlarmLog,PowerCommandParser> {
public:
    HoymilesWClass();

    void loop();

    std::shared_ptr<HoymilesWInverter> addInverter(const char* name, uint64_t serial,const char* hostnameOrIp,uint16_t port);
    std::shared_ptr<HoymilesWInverter> getInverterByPos(uint8_t pos) override;
    std::shared_ptr<HoymilesWInverter> getInverterBySerial(uint64_t serial) override;
    std::shared_ptr<HoymilesWInverter> getInverterBySerialString(const String & serial) override;

    void removeInverterBySerial(uint64_t serial) override;
    size_t getNumInverters() const override;

    bool isAllRadioIdle() const override;

    void init() override;
private:
    std::vector<std::shared_ptr<HoymilesWInverter>> & _inverters;

    std::mutex _mutex;
};

extern HoymilesWClass HoymilesW;
