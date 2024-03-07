//
// Created by lukas on 04.03.24.
//

#include "ServoHandle.h"
#include "Hoymiles.h"
#include "Configuration.h"
#include <iterator>

ServoHandleClass::ServoHandleClass():
_loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&ServoHandleClass::loop, this)){
    _loopTask.setInterval(1 * TASK_SECOND);
    _lastPosition = -1;
    _lastPin = -1;
    _lastFrequency = 0;
    _lastUpdate = 0;
}

void ServoHandleClass::init(Scheduler &scheduler){
    scheduler.addTask(_loopTask);
    _loopTask.enable();

    _lastPosition = Configuration.get().Servo.RangeMin;

    //TODO move this to extra feature
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

int ServoHandleClass::calculatePosition(){

    int setTo = Configuration.get().Servo.RangeMin;

    if(!Configuration.get().Servo.Enabled){
        return setTo;
    }

    if(Configuration.get().Servo.Frequency != _lastFrequency){
        _lastFrequency = Configuration.get().Servo.Frequency;
        ledcSetup(_ledChannel, _lastFrequency, _resolution);
        return setTo;
    }

    if(Configuration.get().Servo.Pin != _lastPin){
        if(_lastPin != -1){
            ledcDetachPin(_lastPin);
        }
        _lastPin = Configuration.get().Servo.Pin;
        ledcAttachPin(_lastPin,_ledChannel);
        ledcWrite(_ledChannel, _lastPosition);
        return setTo;
    }

    Serial.printf("Serial is %llu\n",Configuration.get().Servo.Serial);

    auto inv = Hoymiles.getInverterBySerialString(std::to_string(Configuration.get().Servo.Serial).c_str()).get();
    if(inv == nullptr){
        Serial.printf("Servo -> Serial not found\n");
        return setTo;
    }

    if(!inv->isReachable()){
        Serial.printf("Servo -> Inverter not reachabled\n");
        return setTo;
    }

    if(_lastUpdate == inv->Statistics()->getLastUpdate()){
        return _lastPosition;
        Serial.printf("Servo -> No update\n");
    }
    _lastUpdate = inv->Statistics()->getLastUpdate();

    float value = 0;
    if(Configuration.get().Servo.InputIndex == 0){
        auto c = inv->Statistics()->getChannelsByType(ChannelType_t::TYPE_AC);
        if(c.empty()){
            Serial.printf("Servo -> AC Output not found\n");
            return setTo;
        }
        value = inv->Statistics()->getChannelFieldValue(ChannelType_t::TYPE_AC,*c.begin(), FLD_PAC);
    }else{
        auto c = inv->Statistics()->getChannelsByType(ChannelType_t::TYPE_DC);
        if(c.size() < Configuration.get().Servo.InputIndex){
            Serial.printf("Servo -> DC Input not found\n");
            return setTo;
        }
        auto it = c.begin();
        for(int i = 1; i < Configuration.get().Servo.InputIndex; i++){
            it++;
        }
        value = inv->Statistics()->getChannelFieldValue(ChannelType_t::TYPE_DC,*it, FLD_PDC);
    }

    int dif = Configuration.get().Servo.RangeMax - Configuration.get().Servo.RangeMin;
    float percentage = value / (float)Configuration.get().Servo.Power;
    setTo = (int)(percentage * (float)dif);
    setTo = Configuration.get().Servo.RangeMin + setTo;
    if(Configuration.get().Servo.RangeMax > Configuration.get().Servo.RangeMin){
        setTo = std::min(setTo,(int)Configuration.get().Servo.RangeMax);
        setTo = std::max(setTo,(int)Configuration.get().Servo.RangeMin);
    }else{
        setTo = std::min(setTo,(int)Configuration.get().Servo.RangeMin);
        setTo = std::max(setTo,(int)Configuration.get().Servo.RangeMax);
    }

    Serial.printf("Watt is: %f so set to %d\n",value,setTo);
    return setTo;
}

void ServoHandleClass::loop(){

    int calculatedPosition = calculatePosition();

    if(_lastPosition != calculatedPosition){
        _lastPosition = calculatedPosition;
        Serial.printf("Set servo to %d\n",_lastPosition);
        ledcWrite(_ledChannel, _lastPosition);
    }
}

ServoHandleClass ServoHandle;