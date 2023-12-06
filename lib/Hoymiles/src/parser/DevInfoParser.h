// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Parser.h"
#include "BaseDevInfo.h"

#define DEV_INFO_SIZE 20

class DevInfoParser : public Parser, public BaseDevInfo {
public:
    DevInfoParser();
    void clearBufferAll();
    void appendFragmentAll(uint8_t offset, uint8_t* payload, uint8_t len);

    void clearBufferSimple();
    void appendFragmentSimple(uint8_t offset, uint8_t* payload, uint8_t len);

    uint32_t getLastUpdateAll() override;
    void setLastUpdateAll(uint32_t lastUpdate) override;

    uint32_t getLastUpdateSimple() override;
    void setLastUpdateSimple(uint32_t lastUpdate) override;

    uint16_t getFwBuildVersion() override;
    time_t getFwBuildDateTime() override;
    uint16_t getFwBootloaderVersion() override;

    uint32_t getHwPartNumber() override;
    String getHwVersion() override;

    uint16_t getMaxPower() override;
    String getHwModelName() override;

    bool containsValidData();

private:
    time_t timegm(struct tm* tm);
    uint8_t getDevIdx();

    uint32_t _lastUpdateAll = 0;
    uint32_t _lastUpdateSimple = 0;

    uint8_t _payloadDevInfoAll[DEV_INFO_SIZE] = {};
    uint8_t _devInfoAllLength = 0;

    uint8_t _payloadDevInfoSimple[DEV_INFO_SIZE] = {};
    uint8_t _devInfoSimpleLength = 0;
};