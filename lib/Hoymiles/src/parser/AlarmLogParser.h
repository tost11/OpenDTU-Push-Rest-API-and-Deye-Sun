// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Parser.h"
#include "BaseAlarmLog.h"
#include <array>
#include <cstdint>

#define ALARM_LOG_PAYLOAD_SIZE (ALARM_LOG_ENTRY_COUNT * ALARM_LOG_ENTRY_SIZE + 4)

class AlarmLogParser : public Parser, public BaseAlarmLog {
public:
    AlarmLogParser();
    void clearBuffer() override;
    void appendFragment(const uint8_t offset, const uint8_t* payload, const uint8_t len);

    uint8_t getEntryCount()const override;
    void getLogEntry(const uint8_t entryId, AlarmLogEntry_t& entry, const AlarmMessageLocale_t locale = AlarmMessageLocale_t::EN) override;

    void setLastAlarmRequestSuccess(const LastCommandSuccess status);
    LastCommandSuccess getLastAlarmRequestSuccess() const;

    void setMessageType(const AlarmMessageType_t type);

private:
    static int getTimezoneOffset();
    String getLocaleMessage(const AlarmMessage_t* msg, const AlarmMessageLocale_t locale) const;

    uint8_t _payloadAlarmLog[ALARM_LOG_PAYLOAD_SIZE];
    uint8_t _alarmLogLength = 0;

    LastCommandSuccess _lastAlarmRequestSuccess = CMD_NOK; // Set to NOK to fetch at startup

    AlarmMessageType_t _messageType = AlarmMessageType_t::ALL;
};
