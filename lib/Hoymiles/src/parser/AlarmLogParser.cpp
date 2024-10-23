// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */

/*
This parser is used to parse the response of 'AlarmDataCommand'.

Data structure:
* wcode:
  * right 8 bit: Event ID
  * bit 13: Start time = PM (12h has to be added to start time)
  * bit 12: End time = PM (12h has to be added to start time)
* Start: 12h based start time of the event (PM indicator in wcode)
* End: 12h based start time of the event (PM indicator in wcode)

00   01 02 03 04   05 06 07 08   09   10 11   12 13   14 15   16 17   18 19   20   21   22   23   24 25   26   27 28 29 30 31
                                              00 01   02 03   04 05   06 07   08   09   10   11
                                              |<-------------- First log entry -------------->|   |<->|
-----------------------------------------------------------------------------------------------------------------------------
95   80 14 82 66   80 14 33 28   01   00 01   80 01   00 01   91 EA   91 EA   00   00   00   00   00 8F   65   -- -- -- -- --
^^   ^^^^^^^^^^^   ^^^^^^^^^^^   ^^   ^^^^^   ^^^^^           ^^^^^   ^^^^^   ^^   ^^   ^^   ^^   ^^^^^   ^^
ID   Source Addr   Target Addr   Idx  ?       wcode   ?       Start   End     ?    ?    ?    ?    wcode   CRC8
*/
#include "AlarmLogParser.h"
#include "../Hoymiles.h"
#include <cstring>

AlarmLogParser::AlarmLogParser()
    : Parser()
{
    clearBuffer();
}

void AlarmLogParser::clearBuffer()
{
    memset(_payloadAlarmLog, 0, ALARM_LOG_PAYLOAD_SIZE);
    _alarmLogLength = 0;
}

void AlarmLogParser::appendFragment(const uint8_t offset, const uint8_t* payload, const uint8_t len)
{
    if (offset + len > ALARM_LOG_PAYLOAD_SIZE) {
        Hoymiles.getMessageOutput()->printf("FATAL: (%s, %d) stats packet too large for buffer (%d > %d)\r\n", __FILE__, __LINE__, offset + len, ALARM_LOG_PAYLOAD_SIZE);
        return;
    }
    memcpy(&_payloadAlarmLog[offset], payload, len);
    _alarmLogLength += len;
}

uint8_t AlarmLogParser::getEntryCount() const
{
    if (_alarmLogLength < 2) {
        return 0;
    }
    return (_alarmLogLength - 2) / ALARM_LOG_ENTRY_SIZE;
}

void AlarmLogParser::setLastAlarmRequestSuccess(const LastCommandSuccess status)
{
    _lastAlarmRequestSuccess = status;
}

LastCommandSuccess AlarmLogParser::getLastAlarmRequestSuccess() const
{
    return _lastAlarmRequestSuccess;
}

void AlarmLogParser::setMessageType(const AlarmMessageType_t type)
{
    _messageType = type;
}

void AlarmLogParser::getLogEntry(const uint8_t entryId, AlarmLogEntry_t& entry, const AlarmMessageLocale_t locale)
{
    const uint8_t entryStartOffset = 2 + entryId * ALARM_LOG_ENTRY_SIZE;

    const int timezoneOffset = getTimezoneOffset();

    HOY_SEMAPHORE_TAKE();

    const uint32_t wcode = (uint16_t)_payloadAlarmLog[entryStartOffset] << 8 | _payloadAlarmLog[entryStartOffset + 1];
    uint32_t startTimeOffset = 0;
    if (((wcode >> 13) & 0x01) == 1) {
        startTimeOffset = 12 * 60 * 60;
    }

    uint32_t endTimeOffset = 0;
    if (((wcode >> 12) & 0x01) == 1) {
        endTimeOffset = 12 * 60 * 60;
    }

    entry.MessageId = _payloadAlarmLog[entryStartOffset + 1];
    entry.StartTime = (((uint16_t)_payloadAlarmLog[entryStartOffset + 4] << 8) | ((uint16_t)_payloadAlarmLog[entryStartOffset + 5])) + startTimeOffset + timezoneOffset;
    entry.EndTime = ((uint16_t)_payloadAlarmLog[entryStartOffset + 6] << 8) | ((uint16_t)_payloadAlarmLog[entryStartOffset + 7]);

    HOY_SEMAPHORE_GIVE();

    if (entry.EndTime > 0) {
        entry.EndTime += (endTimeOffset + timezoneOffset);
    }

    switch (locale) {
    case AlarmMessageLocale_t::DE:
        entry.Message = "Unbekannt";
        break;
    case AlarmMessageLocale_t::FR:
        entry.Message = "Inconnu";
        break;
    default:
        entry.Message = "Unknown";
    }

    for (auto& msg : _alarmMessages) {
        if (msg.MessageId == entry.MessageId) {
            if (msg.InverterType == _messageType) {
                entry.Message = getLocaleMessage(&msg, locale);
                break;
            } else if (msg.InverterType == AlarmMessageType_t::ALL) {
                entry.Message = getLocaleMessage(&msg, locale);
            }
        }
    }
}

String AlarmLogParser::getLocaleMessage(const AlarmMessage_t* msg, const AlarmMessageLocale_t locale) const
{
    if (locale == AlarmMessageLocale_t::DE) {
        return msg->Message_de[0] != '\0' ? msg->Message_de : msg->Message_en;
    }

    if (locale == AlarmMessageLocale_t::FR) {
        return msg->Message_fr[0] != '\0' ? msg->Message_fr : msg->Message_en;
    }

    return msg->Message_en;
}

int AlarmLogParser::getTimezoneOffset()
{
    // see: https://stackoverflow.com/questions/13804095/get-the-time-zone-gmt-offset-in-c/44063597#44063597

    time_t gmt, rawtime = time(NULL);
    struct tm* ptm;

    struct tm gbuf;
    ptm = gmtime_r(&rawtime, &gbuf);

    // Request that mktime() looksup dst in timezone database
    ptm->tm_isdst = -1;
    gmt = mktime(ptm);

    return static_cast<int>(difftime(rawtime, gmt));
}