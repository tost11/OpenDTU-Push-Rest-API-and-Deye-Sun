#include "HiFlowBLE.h"
#include "inverters/HiFlowInverter.h"
#include "inverters/HF_2CH.h"

HiFlowBLEClass HiFlowBle;

HiFlowBLEClass::HiFlowBLEClass()
    : _inverters(*reinterpret_cast<std::vector<std::shared_ptr<HiFlowInverter>>*>(&_baseInverters))
{
}

void HiFlowBLEClass::init()
{
    // NimBLE initialization happens lazily on first connect
}

void HiFlowBLEClass::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (getNumInverters() > 0) {
        for (size_t pos = 0; pos < getNumInverters(); pos++) {
            auto inv = getInverterByPos(pos);
            if (inv == nullptr) {
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

std::shared_ptr<HiFlowInverter> HiFlowBLEClass::addInverter(const char* name, uint64_t serial, const char* bleMac)
{
    std::shared_ptr<HiFlowInverter> inv;

    if (HF_2CH::isValidSerial(serial)) {
        inv = std::make_shared<HF_2CH>(serial);
    } else {
        // Default to 2-channel for unknown serials
        inv = std::make_shared<HF_2CH>(serial);
        inv->getDevInfo()->setHardwareModel("HiFlow (Unknown)");
    }

    inv->setName(name);

    // The BLE MAC is stored in bleMac (format "AA:BB:CC:DD:EE:FF")
    // The serial number for V0 pairing is derived from the BLE name (after "RMI-")
    // For now, we use the hex serial as the SN for pairing
    char snHex[13] = {};
    snprintf(snHex, sizeof(snHex), "%04X%08X",
             (uint32_t)((serial >> 32) & 0xFFFF),
             (uint32_t)(serial & 0xFFFFFFFF));

    inv->setBleAddress(bleMac);

    _inverters.push_back(inv);
    inv->startConnection();

    return inv;
}

std::shared_ptr<HiFlowInverter> HiFlowBLEClass::getInverterByPos(uint8_t pos)
{
    if (pos >= _inverters.size()) {
        return nullptr;
    }
    return _inverters[pos];
}

std::shared_ptr<HiFlowInverter> HiFlowBLEClass::getInverterBySerial(uint64_t serial)
{
    for (size_t i = 0; i < _inverters.size(); i++) {
        if (_inverters[i]->serial() == serial) {
            return _inverters[i];
        }
    }
    return nullptr;
}

std::shared_ptr<HiFlowInverter> HiFlowBLEClass::getInverterBySerialString(const String& serial)
{
    for (size_t i = 0; i < _inverters.size(); i++) {
        if (_inverters[i]->serialString() == serial) {
            return _inverters[i];
        }
    }
    return nullptr;
}

void HiFlowBLEClass::removeInverterBySerial(uint64_t serial)
{
    for (size_t i = 0; i < _inverters.size(); i++) {
        if (_inverters[i]->serial() == serial) {
            std::lock_guard<std::mutex> lock(_mutex);
            _inverters.erase(_inverters.begin() + i);
            return;
        }
    }
}

size_t HiFlowBLEClass::getNumInverters() const
{
    return _inverters.size();
}

bool HiFlowBLEClass::isAllRadioIdle() const
{
    return true; // BLE doesn't share radio with NRF/CMT
}
