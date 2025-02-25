#pragma once

#include <vector>
#include "BaseInverterHandler.h"

class InverterHandlerClass {
public:

    bool isAllRadioIdle();
    size_t getNumInverters();

    std::shared_ptr<BaseInverterClass> getInverterByPos(uint8_t pos);
    std::shared_ptr<BaseInverterClass> getInverterBySerial(uint64_t serial,inverter_type inverterTyp);

    //TODO find bettery way to use this (it might be posssible two manufacturers same serial)
    std::shared_ptr<BaseInverterClass> getInverterBySerial(uint64_t serial);
    std::shared_ptr<BaseInverterClass> getInverterBySerialString(const String & serial);

    void init();

    uint32_t PollInterval() const;
    void setPollInterval(uint32_t interval);
    void removeInverterBySerial(uint64_t serial,inverter_type inverterType);
private:

    uint32_t _pollInterval = 0;
    std::vector<BaseInverterHandlerClass*> _handlers;
};

extern InverterHandlerClass InverterHandler;
