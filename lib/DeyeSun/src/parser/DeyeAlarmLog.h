#pragma once

#include "BaseAlarmLog.h"

class DeyeAlarmLog : public BaseAlarmLog {
public:
    uint8_t getEntryCount() override;

    void getLogEntry(uint8_t entryId, AlarmLogEntry_t *entry) override;
};
