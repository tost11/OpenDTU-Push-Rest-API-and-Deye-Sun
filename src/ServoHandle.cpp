//
// Created by lukas on 04.03.24.
//

#include "ServoHandle.h"
#include "Hoymiles.h"

ServoHandleClass::ServoHandleClass():
_loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&ServoHandleClass::loop, this)){
    _loopTask.setInterval(1 * TASK_SECOND);
    _lastPosition = -1;
    _ledPin=15;
    _ledChannel=0;
}

void ServoHandleClass::init(Scheduler &scheduler){
    scheduler.addTask(_loopTask);
    _loopTask.enable();

    // the number of the LED pin
// setting PWM properties
    const int freq = 50;
    const int resolution = 8;
    _lastPosition = 32;

    ledcSetup(_ledChannel, freq, resolution);
    // attach the channel to the GPIO to be controlled
    ledcAttachPin(_ledPin, _ledChannel);
    ledcWrite(_ledChannel, _lastPosition);


    //debug time set
    tm local;
    local.tm_sec = 0;
    local.tm_min = 0;
    local.tm_hour = 0;
    local.tm_mday = 0;
    local.tm_mon = 0;
    local.tm_year = 2024;
    local.tm_isdst = -1;

    time_t t = mktime(&local);
    struct timeval now = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now, NULL);
}

void ServoHandleClass::loop(){
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {

        auto inv = Hoymiles.getInverterByPos(i);
        if (inv->DevInfo()->getLastUpdate() <= 0) {
            continue;
        }

        int setTo = 32;


        if(inv->isReachable()){
            if(_lastUpdate == inv->Statistics()->getLastUpdate()){
                return;
            }
            _lastUpdate = inv->Statistics()->getLastUpdate();
            // Loop all channels
            auto chanels = inv->Statistics()->getChannelsByType(ChannelType_t::TYPE_DC);
            if(chanels.size() > 0){
                auto chanel = chanels.front();
                float val = inv->Statistics()->getChannelFieldValue(ChannelType_t::TYPE_DC,chanel, FLD_PDC);
                //setTo = (int)((val / 400.f) * (32-7) + 7);
                count = count +1;
                Serial.printf("Count is: %d\n",count);
                setTo = count%(33-7);
                setTo +=7;

                Serial.printf("Watt is: %f so set to %d\n",val,setTo);
            }
        }
        if(_lastPosition != setTo){
            _lastPosition = setTo;
            Serial.printf("Set servo to %d\n",setTo);
            ledcWrite(_ledChannel, _lastPosition);
        }
    }
}

ServoHandleClass ServoHandle;