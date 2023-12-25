//
// Created by lukas on 24.11.23.
//

#ifndef OPENDTU_DEYE_SUN_BASEALARMLOG_H
#define OPENDTU_DEYE_SUN_BASEALARMLOG_H

#include <ctime>
#include <WString.h>
#include <cstdint>
#include "Updater.h"

enum class AlarmMessageLocale_t {
    EN,
    DE,
    FR
};

struct AlarmLogEntry_t {
    uint16_t MessageId;
    String Message;
    time_t StartTime;
    time_t EndTime;
};

class BaseAlarmLog : public Updater {
public:
    virtual uint8_t getEntryCount() const = 0;
    virtual void getLogEntry(const uint8_t entryId, AlarmLogEntry_t& entry, const AlarmMessageLocale_t locale) = 0;
};


#endif //OPENDTU_DEYE_SUN_BASEALARMLOG_H
