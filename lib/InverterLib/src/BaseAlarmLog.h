//
// Created by lukas on 24.11.23.
//

#ifndef OPENDTU_DEYE_SUN_BASEALARMLOG_H
#define OPENDTU_DEYE_SUN_BASEALARMLOG_H

#include <ctime>
#include <WString.h>
#include <cstdint>
#include <array>
#include "Updater.h"

#define ALARM_LOG_ENTRY_COUNT 15
#define ALARM_LOG_ENTRY_SIZE 12
#define ALARM_MSG_COUNT 134

struct AlarmLogEntry_t {
    AlarmLogEntry_t(uint16_t messageId, const String &message, time_t startTime, time_t endTime) :
    MessageId(messageId),
    Message(message),
    StartTime(startTime),
    EndTime(endTime) {}
    AlarmLogEntry_t() {}

    uint16_t MessageId;
    String Message;
    time_t StartTime;
    time_t EndTime;
};

enum class AlarmMessageType_t {
    ALL = 0,
    HMT,
    DEYE
};

enum class AlarmMessageLocale_t {
    EN,
    DE,
    FR
};

typedef struct {
    AlarmMessageType_t InverterType;
    uint16_t MessageId;
    const char* Message_en;
    const char* Message_de;
    const char* Message_fr;
} AlarmMessage_t;

class BaseAlarmLog : public Updater {
public:
    virtual uint8_t getEntryCount() const = 0;
    virtual void getLogEntry(const uint8_t entryId, AlarmLogEntry_t& entry, const AlarmMessageLocale_t locale) = 0;
protected:
    static const std::array<const AlarmMessage_t, ALARM_MSG_COUNT> _alarmMessages;
};


#endif //OPENDTU_DEYE_SUN_BASEALARMLOG_H
