#include "HoymilesW.h"
#include "inverters/HoymilesWInverter.h"
#include "inverters/HMS_W_1T.h"
#include "inverters/HMS_W_2T.h"
#include "inverters/HMS_W_3T.h"
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
    std::shared_ptr<HoymilesWInverter> i = nullptr;
    if (HMS_W_1T::isValidSerial(serial)) {
        i = std::make_shared<HMS_W_1T>(serial,*_messageOutput);
    }else if (HMS_W_2T::isValidSerial(serial)) {
        i = std::make_shared<HMS_W_2T>(serial,*_messageOutput);
    }else if (HMS_W_3T::isValidSerial(serial)) {
        i = std::make_shared<HMS_W_3T>(serial,*_messageOutput);
    }else if (HMS_W_4T::isValidSerial(serial)) {
        i = std::make_shared<HMS_W_4T>(serial,*_messageOutput);
    }else{
        i = std::make_shared<HMS_W_4T>(serial,*_messageOutput);
        i->getDevInfo()->setHardwareModel("Unknown");
    }

    if (i) {
        i->setName(name);
        i->setPort(port);
        i->setHostnameOrIpOrMac(hostnameOrIp);
        i->startConnection();
        _inverters.push_back(std::move(i));
        return _inverters.back();
    }

    return nullptr;
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

void HoymilesWClass::setMessageOutput(Print* output)
{
    _messageOutput = output;
}

Print* HoymilesWClass::getMessageOutput()
{
    return _messageOutput;
}

void HoymilesWClass::init() {

}

