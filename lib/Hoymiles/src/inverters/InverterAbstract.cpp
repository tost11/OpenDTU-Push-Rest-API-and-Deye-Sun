// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "InverterAbstract.h"
#include "../Hoymiles.h"
#include "crc.h"
#include <cstring>
#include <MessageOutput.h>

InverterAbstract::InverterAbstract(HoymilesRadio* radio, const uint64_t serial)
{
    _serial.u64 = serial;
    _radio = radio;

    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
        ((uint32_t)((serial >> 32) & 0xFFFFFFFF)),
        ((uint32_t)(serial & 0xFFFFFFFF)));
    _serialString = serial_buff;

    _alarmLogParser.reset(new AlarmLogParser());
    _devInfoParser.reset(new DevInfoParser());
    _powerCommandParser.reset(new PowerCommandParser());
    _statisticsParser.reset(new DefaultStatisticsParser());
}

void InverterAbstract::init()
{
    // This has to be done here because:
    // Not possible in constructor --> virtual function
    // Not possible in verifyAllFragments --> Because no data if nothing is ever received
    // It has to be executed because otherwise the getChannelCount method in stats always returns 0
    getStatistics()->setByteAssignment(getByteAssignment(), getByteAssignmentSize());
}

uint64_t InverterAbstract::serial() const
{
    return _serial.u64;
}

bool InverterAbstract::isProducing()
{
    auto stats = getStatistics();
    float totalAc = 0;
    for (auto& c : stats->getChannelsByType(TYPE_AC)) {
        if (stats->hasChannelFieldValue(TYPE_AC, c, FLD_PAC)) {
            totalAc += stats->getChannelFieldValue(TYPE_AC, c, FLD_PAC);
        }
    }

    return _enablePolling && totalAc > 0;
}

bool InverterAbstract::isReachable()
{
    return _enablePolling && getStatistics()->getRxFailureCount() <= _reachableThreshold;
}

int8_t InverterAbstract::getLastRssi() const
{
    return _lastRssi;
}

bool InverterAbstract::sendChangeChannelRequest()
{
    return false;
}

HoymilesRadio* InverterAbstract::getRadio()
{
    return _radio;
}

void InverterAbstract::clearRxFragmentBuffer()
{
    memset(_rxFragmentBuffer, 0, MAX_RF_FRAGMENT_COUNT * sizeof(fragment_t));
    _rxFragmentMaxPacketId = 0;
    _rxFragmentLastPacketId = 0;
    _rxFragmentRetransmitCnt = 0;
}

void InverterAbstract::addRxFragment(const uint8_t fragment[], const uint8_t len, const int8_t rssi)
{
    _lastRssi = rssi;

    if (len < 11) {
        MessageOutput.printf("FATAL: (%s, %d) fragment too short\r\n", __FILE__, __LINE__);
        return;
    }

    if (len - 11 > MAX_RF_PAYLOAD_SIZE) {
        MessageOutput.printf("FATAL: (%s, %d) fragment too large\r\n", __FILE__, __LINE__);
        return;
    }

    const uint8_t fragmentCount = fragment[9];

    // Packets with 0x81 will be seen as 1
    const uint8_t fragmentId = fragmentCount & 0b01111111; // fragmentId is 1 based

    if (fragmentId == 0) {
        MessageOutput.println("ERROR: fragment id zero received and ignored");
        return;
    }

    if (fragmentId >= MAX_RF_FRAGMENT_COUNT) {
        MessageOutput.printf("ERROR: fragment id %" PRId8 " is too large for buffer and ignored\r\n", fragmentId);
        return;
    }

    memcpy(_rxFragmentBuffer[fragmentId - 1].fragment, &fragment[10], len - 11);
    _rxFragmentBuffer[fragmentId - 1].len = len - 11;
    _rxFragmentBuffer[fragmentId - 1].mainCmd = fragment[0];
    _rxFragmentBuffer[fragmentId - 1].wasReceived = true;

    if (fragmentId > _rxFragmentLastPacketId) {
        _rxFragmentLastPacketId = fragmentId;
    }

    // 0b10000000 == 0x80
    if ((fragmentCount & 0b10000000) == 0b10000000) {
        _rxFragmentMaxPacketId = fragmentId;
    }
}

// Returns Zero on Success or the Fragment ID for retransmit or error code
uint8_t InverterAbstract::verifyAllFragments(CommandAbstract& cmd)
{
    // All missing
    if (_rxFragmentLastPacketId == 0) {
        MessageOutput.println("Hoymiels: All missing");
        if (cmd.getSendCount() <= cmd.getMaxResendCount()) {
            return FRAGMENT_ALL_MISSING_RESEND;
        } else {
            cmd.gotTimeout();
            return FRAGMENT_ALL_MISSING_TIMEOUT;
        }
    }

    // Last fragment is missing (the one with 0x80)
    if (_rxFragmentMaxPacketId == 0) {
        MessageOutput.println("Hoymiels: Last missing");
        if (_rxFragmentRetransmitCnt++ < cmd.getMaxRetransmitCount()) {
            return _rxFragmentLastPacketId + 1;
        } else {
            cmd.gotTimeout();
            return FRAGMENT_RETRANSMIT_TIMEOUT;
        }
    }

    // Middle fragment is missing
    for (uint8_t i = 0; i < _rxFragmentMaxPacketId - 1; i++) {
        if (!_rxFragmentBuffer[i].wasReceived) {
            MessageOutput.println("Hoymiels: Middle missing");
            if (_rxFragmentRetransmitCnt++ < cmd.getMaxRetransmitCount()) {
                return i + 1;
            } else {
                cmd.gotTimeout();
                return FRAGMENT_RETRANSMIT_TIMEOUT;
            }
        }
    }

    if (!cmd.handleResponse(_rxFragmentBuffer, _rxFragmentMaxPacketId)) {
        cmd.gotTimeout();
        return FRAGMENT_HANDLE_ERROR;
    }

    return FRAGMENT_OK;
}

inverter_type InverterAbstract::getInverterType() const {
    return inverter_type::Inverter_Hoymiles;
}

void InverterAbstract::resetRadioStats()
{
    RadioStats = {};
}

void InverterAbstract::performDailyTask()
{
    BaseInverter::performDailyTask();
    resetRadioStats();
}
