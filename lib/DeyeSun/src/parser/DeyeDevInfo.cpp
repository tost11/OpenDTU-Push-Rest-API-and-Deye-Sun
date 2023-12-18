#include <Arduino.h>
#include "DeyeDevInfo.h"
#include "DeyeUtils.h"

uint32_t DeyeDevInfo::getLastUpdateAll() {
    return 0;
}

void DeyeDevInfo::setLastUpdateAll(uint32_t lastUpdate) {

}

uint32_t DeyeDevInfo::getLastUpdateSimple() {
    return 0;
}

void DeyeDevInfo::setLastUpdateSimple(uint32_t lastUpdate) {

}

uint16_t DeyeDevInfo::getFwBootloaderVersion() {
    if(_devInfoLength >= 8){
        return DeyeUtils::defaultParseUInt(6,_payloadDevInfo);
    }
    return 0;
}

uint16_t DeyeDevInfo::getFwBuildVersion() {
    if(_devInfoLength >= 6){
        return DeyeUtils::defaultParseUInt(4,_payloadDevInfo);
    }
    return 0;
}

time_t DeyeDevInfo::getFwBuildDateTime() {
    return 0;
}

uint32_t DeyeDevInfo::getHwPartNumber() {
    return 0;
}

String DeyeDevInfo::getHwVersion() {
    if(_devInfoLength >= 2*5 + 2*5){
        String res = "";
        for(int i=2*5;i<2*5 + 2*5;i++){
            res += _payloadDevInfo[i];
        }
        return res;
    }
    return "";
}

uint16_t DeyeDevInfo::getMaxPower() {
    if(_devInfoLength >= 2){
        return DeyeUtils::defaultParseUInt(0,_payloadDevInfo,_maxPowerDevider);
    }
    return 0;
}

String DeyeDevInfo::getHwModelName() {
    return String();
}

void DeyeDevInfo::clearBuffer() {
    memset(_payloadDevInfo, 0, DEV_INFO_SIZE_DEYE);
    _devInfoLength = 0;
}

void DeyeDevInfo::appendFragment(uint8_t offset, uint8_t *payload, uint8_t len) {
    if (offset + len > DEV_INFO_SIZE_DEYE) {
        Serial.printf("FATAL: (%s, %d) dev info all packet too large for buffer\r\n", __FILE__, __LINE__);
        return;
    }
    memcpy(&_payloadDevInfo[offset], payload, len);
    _devInfoLength += len;
}

void DeyeDevInfo::setMaxPowerDevider(uint8_t dev) {
    _maxPowerDevider = dev;
}
