#pragma once

#include <vector>
#include "BaseInverterHandler.h"

class InverterHandlerClass {
public:

    bool isAllRadioIdle();
    size_t getNumInverters();

    std::shared_ptr<BaseInverterClass> getInverterByPos(uint8_t pos);
    std::shared_ptr<BaseInverterClass> getInverterBySerial(uint64_t serial);

    void init();

    uint32_t PollInterval() const;
    void setPollInterval(uint32_t interval);
    void removeInverterBySerial(uint64_t serial);
private:

    uint32_t _pollInterval = 0;
    std::vector<BaseInverterHandlerClass*> _handlers;
};

extern InverterHandlerClass InverterHandler;
