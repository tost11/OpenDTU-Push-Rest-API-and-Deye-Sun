#pragma once

#include <Print.h>
#include <memory>
#include <vector>
#include <mutex>
#include <HardwareSerial.h>
#include "BaseInverterHandler.h"
#include "inverters/DeyeInverter.h"

class DeyeSunClass: public BaseInverterHandler<DeyeInverter,DefaultStatisticsParser,DeyeDevInfo,DeyeAlarmLog,PowerCommandParser> {
public:
    DeyeSunClass();

    void loop();

    std::shared_ptr<DeyeInverter> addInverter(const char* name, uint64_t serial,const char* hostnameOrIp,uint16_t port);
    std::shared_ptr<DeyeInverter> getInverterByPos(uint8_t pos) override;
    std::shared_ptr<DeyeInverter> getInverterBySerial(uint64_t serial) override;
    std::shared_ptr<DeyeInverter> getInverterBySerialString(const String & serial) override;
    void removeInverterBySerial(uint64_t serial) override;
    size_t getNumInverters() const override;

    bool isAllRadioIdle() const override;

    void init() override;

    void setUnknownDevicesWriteEnable(bool enabled);
    bool getUnknownDevicesWriteEnable()const;
private:
    std::vector<std::shared_ptr<DeyeInverter>> & _inverters;

    std::mutex _mutex;

    uint32_t _pollInterval = 0;
    uint32_t _lastPoll = 0;

    bool _unknownDeviceWriteEnabled;
};

extern DeyeSunClass DeyeSun;
