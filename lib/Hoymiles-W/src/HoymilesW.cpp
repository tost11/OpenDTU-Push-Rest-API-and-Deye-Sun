#include "HoymilesW.h"
#include "inverters/HoymilesWInverter.h"
#include "inverters/HMS_W_1T.h"
#include "inverters/HMS_W_2T.h"
#include "inverters/HMS_W_4T.h"

HoymilesWClass HoymilesW;

HoymilesWClass::HoymilesWClass():
_inverters(*reinterpret_cast<std::vector<std::shared_ptr<HoymilesWInverter>>*>(&_baseInverters)){

}

void HoymilesWClass::loop()
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

std::shared_ptr<HoymilesWInverter> HoymilesWClass::addInverter(const char* name, uint64_t serial,const char* hostnameOrIp,uint16_t port)
{
    std::shared_ptr<HoymilesWInverter> i;
    if (HMS_W_1T::isValidSerial(serial)) {
        i = std::make_shared<HMS_W_1T>(serial);
    }else if (HMS_W_2T::isValidSerial(serial)) {
        i = std::make_shared<HMS_W_2T>(serial);
    }else if (HMS_W_4T::isValidSerial(serial)) {
        i = std::make_shared<HMS_W_4T>(serial);
    }else{
        i = std::make_shared<HMS_W_4T>(serial);
        i->getDevInfo()->setHardwareModel("Unknown");
    }

    i->setName(name);
    i->setHostnameOrIpOrMacAndPort(hostnameOrIp,port);
    i->startConnection();
    _inverters.push_back(i);

    return i;
}

std::shared_ptr<HoymilesWInverter> HoymilesWClass::getInverterByPos(uint8_t pos)
{
    if (pos >= _inverters.size()) {
        return nullptr;
    } else {
        return _inverters[pos];
    }
}

std::shared_ptr<HoymilesWInverter> HoymilesWClass::getInverterBySerial(uint64_t serial)
{
    for (uint8_t i = 0; i < _inverters.size(); i++) {
        if (_inverters[i]->serial() == serial) {
            return _inverters[i];
        }
    }
    return nullptr;
}

std::shared_ptr<HoymilesWInverter> HoymilesWClass::getInverterBySerialString(const String & serial)
{
    for (uint8_t i = 0; i < _inverters.size(); i++) {
        if (_inverters[i]->serialString() == serial) {
            return _inverters[i];
        }
    }
    return nullptr;
}

void HoymilesWClass::removeInverterBySerial(uint64_t serial)
{
    for (uint8_t i = 0; i < _inverters.size(); i++) {
        if (_inverters[i]->serial() == serial) {
            std::lock_guard<std::mutex> lock(_mutex);
            _inverters.erase(_inverters.begin() + i);
            return;
        }
    }
}

size_t HoymilesWClass::getNumInverters() const
{
    return _inverters.size();
}

bool HoymilesWClass::isAllRadioIdle() const
{
    //TODO
    return true;
}

void HoymilesWClass::init() {

}

