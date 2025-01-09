#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include "BaseInverterHandler.h"
#include "inverters/DeyeInverter.h"

class Print;

class DeyeSunClass: public BaseInverterHandler<DeyeInverter,DefaultStatisticsParser,DeyeDevInfo,DeyeSystemConfigPara,DeyeAlarmLog,DeyeGridProfile,PowerCommandParser> {
public:
    DeyeSunClass();

    void loop();

    void setMessageOutput(Print* output);
    Print* getMessageOutput();

    std::shared_ptr<DeyeInverter> addInverter(const char* name, uint64_t serial,const char* hostnameOrIp,uint16_t port);
    std::shared_ptr<DeyeInverter> getInverterByPos(uint8_t pos) override;
    std::shared_ptr<DeyeInverter> getInverterBySerial(uint64_t serial) override;
    std::shared_ptr<DeyeInverter> getInverterBySerialString(const String & serialString);
    void removeInverterBySerial(uint64_t serial) override;
    size_t getNumInverters() const override;

    bool isAllRadioIdle() const override;

    void init() override;

private:
    std::vector<std::shared_ptr<DeyeInverter>> & _inverters;

    std::mutex _mutex;

    uint32_t _pollInterval = 0;
    uint32_t _lastPoll = 0;

    Print* _messageOutput = &Serial;
};

extern DeyeSunClass DeyeSun;
