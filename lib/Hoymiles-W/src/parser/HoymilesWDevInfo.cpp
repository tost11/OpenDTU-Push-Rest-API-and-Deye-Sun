#include <Arduino.h>
#include "HoymilesWDevInfo.h"

uint32_t HoymilesWDevInfo::getLastUpdateAll() const {
    return 0;
}

void HoymilesWDevInfo::setLastUpdateAll(uint32_t lastUpdate) {

}

uint32_t HoymilesWDevInfo::getLastUpdateSimple() const {
    return 0;
}

void HoymilesWDevInfo::setLastUpdateSimple(uint32_t lastUpdate) {

}

uint16_t HoymilesWDevInfo::getFwBootloaderVersion() const {
    return 0;
}

uint16_t HoymilesWDevInfo::getFwBuildVersion() const {
    return 0;
}

time_t HoymilesWDevInfo::getFwBuildDateTime() const {
    return 0;
}

String HoymilesWDevInfo::getFwBuildDateTimeStr() const{
    return "";
};

uint32_t HoymilesWDevInfo::getHwPartNumber() const {
    return 0;
}

String HoymilesWDevInfo::getHwVersion() const {
    return _hardwareVersion;
}

uint16_t HoymilesWDevInfo::getMaxPower() const {
    return _maxPower;
}

String HoymilesWDevInfo::getHwModelName() const {
    return _hardwareModel;
}

void HoymilesWDevInfo::clearBuffer() {
    memset(_payloadDevInfo, 0, DEV_INFO_SIZE_HOYMILES_W);
    _devInfoLength = 0;
}

void HoymilesWDevInfo::appendFragment(uint8_t offset, uint8_t *payload, uint8_t len) {
    if (offset + len > DEV_INFO_SIZE_HOYMILES_W) {
        Serial.printf("FATAL: (%s, %d) dev info all packet too large for buffer\r\n", __FILE__, __LINE__);
        return;
    }
    memcpy(&_payloadDevInfo[offset], payload, len);
    _devInfoLength += len;
}

void HoymilesWDevInfo::setMaxPowerDevider(uint8_t dev) {
    _maxPowerDevider = dev;
}

void HoymilesWDevInfo::setHardwareVersion(const String &version) {
    _hardwareVersion = version;
}

void HoymilesWDevInfo::setHardwareModel(const String &model) {
    _hardwareModel = model;
}

void HoymilesWDevInfo::setMaxPower(const uint16_t maxPower){
    _maxPower = maxPower;
}