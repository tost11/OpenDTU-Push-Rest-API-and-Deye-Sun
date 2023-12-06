
#include "InverterHandler.h"
#include "Hoymiles.h"
#include "DeyeSun.h"

InverterHandlerClass InverterHandler;

bool InverterHandlerClass::isAllRadioIdle() {

    for (const auto &item: _handlers){
        if(!item->isAllRadioIdle()){
            return false;
        }
    }

    return true;
}

void InverterHandlerClass::init() {
    _handlers.push_back(reinterpret_cast<BaseInverterHandlerClass*>(&Hoymiles));
    _handlers.push_back(reinterpret_cast<BaseInverterHandlerClass*>(&DeyeSun));
}

size_t InverterHandlerClass::getNumInverters() {
    size_t res = 0;
    for (const auto &item: _handlers){
        res+=item->getNumInverters();
    }
    return res;
}

std::shared_ptr<BaseInverterClass> InverterHandlerClass::getInverterByPos(uint8_t pos) {
    int i=0;
    for (const auto &item: _handlers){
        for(int j = 0;j<item->getNumInverters();j++){
            if(i == pos){
                return item->getInverterByPos(j);
            }
            i++;
        }
    }
    return nullptr;
}

uint32_t InverterHandlerClass::PollInterval() const {
    return _pollInterval;
}

void InverterHandlerClass::setPollInterval(uint32_t interval) {
    for (const auto &item: _handlers){
        item->setPollInterval(interval);
    }
}

std::shared_ptr<BaseInverterClass> InverterHandlerClass::getInverterBySerial(uint64_t serial) {
    for (const auto &item: _handlers){
        auto res = item->getInverterBySerial(serial);
        if(res != nullptr){
            return res;
        }
    }
    return nullptr;
}

void InverterHandlerClass::removeInverterBySerial(uint64_t serial) {
    for (const auto &item: _handlers){
        if(item->getInverterBySerial(serial) != nullptr){
            item->removeInverterBySerial(serial);
            return;
        }
    }
}
