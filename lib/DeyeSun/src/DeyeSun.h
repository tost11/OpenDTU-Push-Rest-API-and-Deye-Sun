#pragma once

#include <Print.h>
#include <memory>
#include <vector>
#include <mutex>
#include <HardwareSerial.h>
#include "BaseInverterHandler.h"
#include "parser/DeyeDevInfo.h"
#include "parser/DeyeSystemConfigPara.h"
#include "parser/DeyeAlarmLog.h"
#include "parser/DeyeGridProfile.h"
#include "parser/DeyePowerCommand.h"
#include "parser/StatisticsParser.h"
#include "inverters/DeyeInverter.h"

class DeyeSunClass: public BaseInverterHandler<DeyeInverter,DeyeStatistics,DeyeDevInfo,DeyeSystemConfigPara,DeyeAlarmLog,DeyeGridProfile,DeyePowerCommand> {
public:
    void loop();

    void setMessageOutput(Print* output);
    Print* getMessageOutput();

    std::shared_ptr<DeyeInverter> addInverter(const char* name, uint64_t serial,const char* hostnameOrIp,uint16_t port);
    std::shared_ptr<DeyeInverter> getInverterByPos(uint8_t pos) override;
    std::shared_ptr<DeyeInverter> getInverterBySerial(uint64_t serial) override;
    void removeInverterBySerial(uint64_t serial) override;
    size_t getNumInverters() const override;

    bool isAllRadioIdle() const override;

    void init() override;

private:
    std::vector<std::shared_ptr<DeyeInverter>> _inverters;

    std::mutex _mutex;

    uint32_t _pollInterval = 0;
    uint32_t _lastPoll = 0;

    Print* _messageOutput = &Serial;
};

extern DeyeSunClass DeyeSun;
