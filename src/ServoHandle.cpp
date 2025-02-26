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
    _lastFrequency = 0;
    _lastUpdate = 0;
    _selfTestStep = -1;
    _selfTestTimer.set(1000);
}

void ServoHandleClass::init(Scheduler &scheduler,uint8_t pin){
    scheduler.addTask(_loopTask);
    _loopTask.enable();

    _lastPosition = Configuration.get().Servo.RangeMin;
    _lastFrequency = Configuration.get().Servo.Frequency;
    _lastResolution = Configuration.get().Servo.Resolution;

    _pin = pin;

    if(_pin > 0){
        ledcSetup(_ledChannel, _lastFrequency, _lastResolution);
        ledcWrite(_ledChannel, _lastPosition);
        ledcAttachPin(pin,_ledChannel);
    }

    startSelfTest();
}

void ServoHandleClass::startSelfTest(){
    _selfTestStep = Configuration.get().Servo.RangeMin;
    _selfTestTimer.reset();
    _lastPosition = Configuration.get().Servo.RangeMax;//so to max so it is different (will be overwritten)
}

int ServoHandleClass::handleSelfTest(){
    Serial.printf("Handle self test\n");
    if(_selfTestTimer.occured()){
        Serial.printf("Test occured\n");
        _selfTestTimer.reset();
        if(Configuration.get().Servo.RangeMin > Configuration.get().Servo.RangeMax){
            _selfTestStep--;
            if(_selfTestStep < Configuration.get().Servo.RangeMax){
                _selfTestStep = -1;
                return Configuration.get().Servo.RangeMin;
            }
        }else{
            _selfTestStep++;
            if(_selfTestStep > Configuration.get().Servo.RangeMax){
                _selfTestStep = -1;
                return Configuration.get().Servo.RangeMin;
            }
        }
        return _selfTestStep;
    }
    return _lastPosition;
}

int ServoHandleClass::calculatePosition(){

    if(_selfTestStep >= 0){
        return handleSelfTest();
    }

    int setTo = Configuration.get().Servo.RangeMin;

    if(_pin <= 0){
        return setTo;
    }

    if(Configuration.get().Servo.Frequency != _lastFrequency ||
            Configuration.get().Servo.Resolution != _lastResolution){
        _lastFrequency = Configuration.get().Servo.Frequency;
        _lastResolution = Configuration.get().Servo.Frequency;
        ledcSetup(_ledChannel, _lastFrequency, _lastResolution);
        ledcWrite(_ledChannel, _lastPosition);
        return setTo;
    }

    InverterAbstract * inv = nullptr;
    if(Configuration.get().Servo.Serial == 0){
        for(int i=0;i<Hoymiles.getNumInverters();i++){
            inv = Hoymiles.getInverterByPos(i).get();
            if(inv != nullptr){
                break;
            }
        }
    }else{
        inv = Hoymiles.getInverterBySerialString(std::to_string(Configuration.get().Servo.Serial).c_str()).get();
    }

    if(inv == nullptr){
        Serial.printf("Inverter not found by Serial or first one\n");
        return setTo;
    }

    if(!inv->isReachable()){
        Serial.printf("Servo -> Inverter not reachabled\n");
        return setTo;
    }

    if(_lastUpdate == inv->getStatistics()->getLastUpdate()){
        return _lastPosition;
        Serial.printf("Servo -> No update\n");
    }
    _lastUpdate = inv->getStatistics()->getLastUpdate();

    float value = 0;
    if(Configuration.get().Servo.InputIndex == 0){
        auto c = inv->getStatistics()->getChannelsByType(ChannelType_t::TYPE_AC);
        if(c.empty()){
            Serial.printf("Servo -> AC Output not found\n");
            return setTo;
        }
        value = inv->getStatistics()->getChannelFieldValue(ChannelType_t::TYPE_AC,*c.begin(), FLD_PAC);
    }else{
        auto c = inv->getStatistics()->getChannelsByType(ChannelType_t::TYPE_DC);
        if(c.size() < Configuration.get().Servo.InputIndex){
            Serial.printf("Servo -> DC Input not found\n");
            return setTo;
        }
        auto it = c.begin();
        for(int i = 1; i < Configuration.get().Servo.InputIndex; i++){
            it++;
        }
        value = inv->getStatistics()->getChannelFieldValue(ChannelType_t::TYPE_DC,*it, FLD_PDC);
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
