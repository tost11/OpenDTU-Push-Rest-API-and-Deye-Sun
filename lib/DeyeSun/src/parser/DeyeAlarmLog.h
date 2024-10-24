#pragma once

#include <list>
#include <vector>
#include "BaseAlarmLog.h"

class DeyeAlarmLog : public BaseAlarmLog {
private:
    std::vector<AlarmLogEntry_t> _errors;
public:
    void clearBuffer() override;
    uint8_t getEntryCount() const override;

    void getLogEntry(const uint8_t entryId, AlarmLogEntry_t& entry, const AlarmMessageLocale_t locale = AlarmMessageLocale_t::EN) override;

    void addAlarm(uint16_t id,time_t start,time_t end,const String & message = "");
    void addAlarm(uint16_t id,uint32_t sec,const String & message = "");

    void checkErrorsForTimeout();
};
