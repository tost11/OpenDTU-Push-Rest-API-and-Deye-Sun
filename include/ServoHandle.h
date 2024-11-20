#pragma once

#include <TaskSchedulerDeclarations.h>

class ServoHandleClass{
public:
    ServoHandleClass();
    void init(Scheduler &scheduler,uint8_t pin);

private:
    Task _loopTask;

    uint32_t _lastUpdate;
    void loop();
    int calculatePosition();

    const int _ledChannel = 0;

    int _lastPosition;
    int _lastFrequency;
    int _lastResolution;
    uint8_t _pin;
};

extern ServoHandleClass ServoHandle;