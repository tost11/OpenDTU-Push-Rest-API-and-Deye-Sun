
#include "InverterHandler.h"

#ifdef HOYMILES
#include "Hoymiles.h"
#endif

#ifdef DEYE_SUN
#include "DeyeSun.h"
#endif

#ifdef HOYMILES_W
#include "HoymilesW.h"
#endif

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
    #ifdef HOYMILES
    _handlers.push_back(reinterpret_cast<BaseInverterHandlerClass*>(&Hoymiles));
    #endif
    #ifdef DEYE_SUN
    _handlers.push_back(reinterpret_cast<BaseInverterHandlerClass*>(&DeyeSun));
    #endif
    #ifdef HOYMILES_W
    _handlers.push_back(reinterpret_cast<BaseInverterHandlerClass*>(&HoymilesW));
    #endif
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
        auto ret = item->getInverterBySerial(serial);
        if(ret != nullptr){
            return ret;
        }
    }
    return nullptr;
}


std::shared_ptr<BaseInverterClass> InverterHandlerClass::getInverterBySerialString(const String & serial) {
    for (const auto &item: _handlers){
        auto ret = item->getInverterBySerialString(serial);
        if(ret != nullptr){
            return ret;
        }
    }
    return nullptr;
}

std::shared_ptr<BaseInverterClass> InverterHandlerClass::getInverterBySerial(uint64_t serial,inverter_type inverterType) {
    #ifdef HOYMILES
    if(inverterType == inverter_type::Inverter_Hoymiles){
        return std::reinterpret_pointer_cast<BaseInverterClass>(Hoymiles.getInverterBySerial(serial));
    }
    #endif
    #ifdef DEYE_SUN
    if(inverterType == inverter_type::Inverter_DeyeSun){
        return std::reinterpret_pointer_cast<BaseInverterClass>(DeyeSun.getInverterBySerial(serial));
    }
    #endif
    #ifdef HOYMILES_W
    if(inverterType == inverter_type::Inverter_HoymilesW){
        return std::reinterpret_pointer_cast<BaseInverterClass>(HoymilesW.getInverterBySerial(serial));
    }
    #endif
    return nullptr;
}

void InverterHandlerClass::removeInverterBySerial(uint64_t serial,inverter_type inverterType) {
    #ifdef HOYMILES
    if(inverterType == inverter_type::Inverter_Hoymiles){
        Hoymiles.removeInverterBySerial(serial);
    }
    #endif
    #ifdef DEYE_SUN
    if(inverterType == Inverter_DeyeSun){
        DeyeSun.removeInverterBySerial(serial);
    }
    #endif
    #ifdef HOYMILES_W
    if(inverterType == Inverter_HoymilesW){
        HoymilesW.removeInverterBySerial(serial);
    }
    #endif
}
