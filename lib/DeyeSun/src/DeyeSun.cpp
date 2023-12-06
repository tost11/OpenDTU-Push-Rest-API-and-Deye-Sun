#include "DeyeSun.h"

DeyeSunClass DeyeSun;

void DeyeSunClass::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (getNumInverters() > 0) {
        if (millis() - _lastPoll > (_pollInterval * 1000)) {

            _lastPoll = millis();

            for(size_t pos = 0; pos <= getNumInverters(); pos++){


                auto inv = getInverterByPos(0);
                if(inv == nullptr){
                    continue;
                }

                float testValue = 150.f;

                uint8_t toAdd[4];

                memcpy(toAdd,&testValue,4);

                inv->Statistics()->beginAppendFragment();
                inv->Statistics()->clearBuffer();
                inv->Statistics()->appendFragment(0,toAdd,4);
                inv->Statistics()->endAppendFragment();
                inv->Statistics()->resetRxFailureCount();
                inv->Statistics()->setLastUpdate(millis());

                inv->updateSocket();
            }
        }
    }
}

std::shared_ptr<DeyeInverter> DeyeSunClass::addInverter(const char* name, uint64_t serial,const char* hostnameOrIp,uint16_t port)
{
    std::shared_ptr<DeyeInverter> i = nullptr;

    //TODO validate serial

    i = std::make_unique<DeyeInverter>(serial);

    if (i) {
        i->setName(name);
        i->setPort(port);
        i->setHostnameOrIp(hostnameOrIp);
        _inverters.push_back(std::move(i));
        return _inverters.back();
    }

    return nullptr;
}

std::shared_ptr<DeyeInverter> DeyeSunClass::getInverterByPos(uint8_t pos)
{
    if (pos >= _inverters.size()) {
        return nullptr;
    } else {
        return _inverters[pos];
    }
}

std::shared_ptr<DeyeInverter> DeyeSunClass::getInverterBySerial(uint64_t serial)
{
    for (uint8_t i = 0; i < _inverters.size(); i++) {
        if (_inverters[i]->serial() == serial) {
            return _inverters[i];
        }
    }
    return nullptr;
}

void DeyeSunClass::removeInverterBySerial(uint64_t serial)
{
    for (uint8_t i = 0; i < _inverters.size(); i++) {
        if (_inverters[i]->serial() == serial) {
            std::lock_guard<std::mutex> lock(_mutex);
            _inverters.erase(_inverters.begin() + i);
            return;
        }
    }
}

size_t DeyeSunClass::getNumInverters()
{
    return _inverters.size();
}


bool DeyeSunClass::isAllRadioIdle()
{
    //TODO
    return false;
}

void DeyeSunClass::setMessageOutput(Print* output)
{
    _messageOutput = output;
}

Print* DeyeSunClass::getMessageOutput()
{
    return _messageOutput;
}

void DeyeSunClass::init() {

}
