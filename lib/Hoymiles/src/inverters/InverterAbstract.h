// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "../commands/ActivePowerControlCommand.h"
#include "../parser/AlarmLogParser.h"
#include "../parser/DevInfoParser.h"
#include "../parser/PowerCommandParser.h"
#include <parser/DefaultStatisticsParser.h>
#include "HoymilesRadio.h"
#include "types.h"
#include <inverter/BaseInverter.h>
#include <Arduino.h>
#include <cstdint>
#include <list>

enum {
    FRAGMENT_ALL_MISSING_RESEND = 255,
    FRAGMENT_ALL_MISSING_TIMEOUT = 254,
    FRAGMENT_RETRANSMIT_TIMEOUT = 253,
    FRAGMENT_HANDLE_ERROR = 252,
    FRAGMENT_OK = 0
};

#define MAX_RF_FRAGMENT_COUNT 13

class CommandAbstract;

class InverterAbstract : public BaseInverter<DefaultStatisticsParser,DevInfoParser,AlarmLogParser,PowerCommandParser> {
public:
    explicit InverterAbstract(HoymilesRadio* radio, const uint64_t serial);
    void init();
    uint64_t serial() const override;
    virtual const byteAssign_t* getByteAssignment() const = 0;
    virtual uint8_t getByteAssignmentSize() const = 0;

    bool isProducing() override;
    bool isReachable() override;

    int8_t getLastRssi() const;

    void clearRxFragmentBuffer();
    void addRxFragment(const uint8_t fragment[], const uint8_t len, const int8_t rssi);
    uint8_t verifyAllFragments(CommandAbstract& cmd);

    void resetRadioStats();

    void resetStats() override;

    struct {
        // TX Request Data
        uint32_t TxRequestData;

        // TX Re-Request Fragment
        uint32_t TxReRequestFragment;

        // RX Success
        uint32_t RxSuccess;

        // RX Fail Partial Answer
        uint32_t RxFailPartialAnswer;

        // RX Fail No Answer
        uint32_t RxFailNoAnswer;

        // RX Fail Corrupt Data
        uint32_t RxFailCorruptData;
    } RadioStats = {};

    virtual bool sendStatsRequest() = 0;
    virtual bool sendAlarmLogRequest(const bool force = false) = 0;
    virtual bool sendDevInfoRequest() = 0;
    virtual bool sendSystemConfigParaRequest() = 0;
    //virtual bool sendActivePowerControlRequest(float limit, const PowerLimitControlType type) = 0;
    virtual bool resendActivePowerControlRequest() = 0;
    //virtual bool sendPowerControlRequest(const bool turnOn) = 0;
    //virtual bool sendRestartControlRequest() = 0;
    //virtual bool resendPowerControlRequest() = 0;
    virtual bool sendChangeChannelRequest();
    virtual bool sendGridOnProFileParaRequest() = 0;

    inverter_type getInverterType() const override;

    HoymilesRadio* getRadio();
protected:
    HoymilesRadio* _radio;

private:
    serial_u _serial;

    int8_t _lastRssi = -127;

    fragment_t _rxFragmentBuffer[MAX_RF_FRAGMENT_COUNT];
    uint8_t _rxFragmentMaxPacketId = 0;
    uint8_t _rxFragmentLastPacketId = 0;
    uint8_t _rxFragmentRetransmitCnt = 0;
};
