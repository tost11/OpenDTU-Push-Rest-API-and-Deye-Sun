#include "DeyeSun.h"
#include "inverters/DeyeInverter.h"
#include "inverters/DS_1CH.h"
#include "inverters/DS_2CH.h"

DeyeSunClass DeyeSun;

DeyeSunClass::DeyeSunClass():
_inverters(*reinterpret_cast<std::vector<std::shared_ptr<DeyeInverter>>*>(&_baseInverters)){

}

void DeyeSunClass::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (getNumInverters() > 0) {
        for(size_t pos = 0; pos <= getNumInverters(); pos++){
            auto inv = getInverterByPos(pos);
            if(inv == nullptr){
                continue;
            }

            if (inv->getZeroValuesIfUnreachable() && !inv->isReachable()) {
                inv->getStatistics()->zeroRuntimeData();
            }

            if (inv->getEnablePolling() || inv->getEnableCommands()) {
                inv->update();
            }
        }
    }

    performHouseKeeping();
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
        i->setHostnameOrIpOrMac(hostnameOrIp);
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

std::shared_ptr<DeyeInverter> DeyeSunClass::getInverterBySerialString(const String & serial)
{
    for (uint8_t i = 0; i < _inverters.size(); i++) {
        if (_inverters[i]->serialString() == serial) {
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


void DeyeSunClass::init() {

}

