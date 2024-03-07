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

    int count=0;

    int _ledPin;  // 16 corresponds to GPIO16
    int _ledChannel;

    int _lastPosition;
};

extern ServoHandleClass ServoHandle;