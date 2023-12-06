//
// Created by lukas on 24.11.23.
//

#ifndef OPENDTU_DEYE_SUN_BASEALARMLOG_H
#define OPENDTU_DEYE_SUN_BASEALARMLOG_H

#include <ctime>
#include <WString.h>

struct AlarmLogEntry_t {
    uint16_t MessageId;
    String Message;
    time_t StartTime;
    time_t EndTime;
};

#include <cstdint>
#include "Updater.h"

class BaseAlarmLog : public Updater {
public:
    virtual uint8_t getEntryCount() = 0;
    virtual void getLogEntry(uint8_t entryId, AlarmLogEntry_t* entry) = 0;
};


#endif //OPENDTU_DEYE_SUN_BASEALARMLOG_H
