#include "DeyeSun.h"
#include "inverters/DeyeInverter.h"
#include "inverters/DS_1CH.h"
#include "inverters/DS_2CH.h"

DeyeSunClass DeyeSun;

void DeyeSunClass::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (getNumInverters() > 0) {
        for(size_t pos = 0; pos <= getNumInverters(); pos++){


            auto inv = getInverterByPos(0);
            if(inv == nullptr){
                continue;
            }

            /*float testValue = 150.f;

            uint8_t toAdd[4];

            memcpy(toAdd,&testValue,4);

            inv->Statistics()->beginAppendFragment();
            inv->Statistics()->clearBuffer();
            inv->Statistics()->appendFragment(0,toAdd,4);
            inv->Statistics()->endAppendFragment();
            inv->Statistics()->resetRxFailureCount();
            inv->Statistics()->setLastUpdate(millis());

             */
            inv->updateSocket();
        }
    }
}

std::shared_ptr<DeyeInverter> DeyeSunClass::addInverter(const char* name, uint64_t serial,const char* hostnameOrIp,uint16_t port)
{
    std::shared_ptr<DeyeInverter> i = nullptr;

    String type = DeyeInverter::serialToModel(serial);

    if(type.startsWith("SUN300G3")){
        i = std::reinterpret_pointer_cast<DeyeInverter>(std::make_shared<DS_1CH>(serial,type));
    }else{
        i = std::reinterpret_pointer_cast<DeyeInverter>(std::make_shared<DS_2CH>(serial,type));
    }

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

size_t DeyeSunClass::getNumInverters() const
{
    return _inverters.size();
}


bool DeyeSunClass::isAllRadioIdle() const
{
    //TODO
    return true;
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
