#pragma once

#include <TaskSchedulerDeclarations.h>

class ServoHandleClass{
public:
    ServoHandleClass();
    void init(Scheduler &scheduler);

private:
    Task _loopTask;

    uint32_t _lastUpdate;
    void loop();
    int calculatePosition();

    const int _ledChannel = 0;
    const int _resolution = 8;

    int _lastPosition;
    int _lastFrequency;
    int _lastPin;
};

extern ServoHandleClass ServoHandle;