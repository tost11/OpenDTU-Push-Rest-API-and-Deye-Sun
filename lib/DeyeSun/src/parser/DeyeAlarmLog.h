#pragma once

#include "BaseAlarmLog.h"

class DeyeAlarmLog : public BaseAlarmLog {
public:
    uint8_t getEntryCount() const override;

    void getLogEntry(const uint8_t entryId, AlarmLogEntry_t& entry, const AlarmMessageLocale_t locale = AlarmMessageLocale_t::EN) override;
};
