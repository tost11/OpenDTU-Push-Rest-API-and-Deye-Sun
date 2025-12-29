#pragma once

#include <TaskSchedulerDeclarations.h>
#include <TimeoutHelper.h>

class ServoHandleClass{
public:
    ServoHandleClass();
    void init(Scheduler &scheduler);

    void startSelfTest();

private:
    Task _loopTask;

    uint32_t _lastUpdate;
    void loop();
    int calculatePosition();
    int handleSelfTest();

    const int _ledChannel = 0;

    int _lastPosition;
    int _lastFrequency;
    int _lastResolution;
    uint8_t _pin;

    int _selfTestStep;
    TimeoutHelper _selfTestTimer;
};

extern ServoHandleClass ServoHandle;
