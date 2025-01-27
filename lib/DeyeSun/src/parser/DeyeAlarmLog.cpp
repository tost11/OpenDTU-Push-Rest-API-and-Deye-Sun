#include "DeyeAlarmLog.h"

#include <Arduino.h>
#include <MessageOutput.h>

uint8_t DeyeAlarmLog::getEntryCount() const {
    return _errors.size();
}

void DeyeAlarmLog::getLogEntry(const uint8_t entryId, AlarmLogEntry_t &entry, const AlarmMessageLocale_t locale) {
    assert(entryId <= _errors.size());
    auto & ret = _errors[entryId];

    entry.EndTime = ret.EndTime;
    entry.StartTime = ret.StartTime;
    entry.Message = ret.Message;
    entry.MessageId = ret.MessageId;

    auto & orig = _alarmMessages[ret.MessageId-1];
    if(entry.Message == ""){
        if(locale == AlarmMessageLocale_t::FR){
            entry.Message = orig.Message_fr;
        }else if(locale == AlarmMessageLocale_t::DE){
            entry.Message =orig.Message_de;
        }else{
            entry.Message = orig.Message_en;
        }
    }

    if(entry.Message == ""){
        entry.Message = orig.Message_en;
    }
}

void DeyeAlarmLog::addAlarm(uint16_t id, time_t start, time_t end,const String & message) {
    for (auto &item: _errors){
        if(item.MessageId == id) {
            item.StartTime = start;
            item.EndTime = end;
            item.Message = message;
            return;
        }
    }

    //remove oldest entry
    if(_errors.size() >= ALARM_LOG_ENTRY_COUNT){
        MessageOutput.println("Deye Sun: Alert que full -> removed first one (oldest) Removed alert");
        _errors.erase(_errors.begin());
    }

    _errors.emplace_back(id,message,start,end);
}

void DeyeAlarmLog::checkErrorsForTimeout() {

    //TODO timer do not check so often

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo,5)) {
        return;
    }


    auto it = _errors.begin();
    while(it != _errors.end()){
        if(timeinfo.tm_sec > it->EndTime){
            it = _errors.erase(it);
            MessageOutput.println("Deye Sun: Removed alert");
            continue;
        }
        it++;
    }
}

void DeyeAlarmLog::addAlarm(uint16_t id, uint32_t sec, const String &message) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo,5)) {
        return;
    }

    addAlarm(id,timeinfo.tm_sec,timeinfo.tm_sec+sec,message);
}


void DeyeAlarmLog::clearBuffer(){
    _errors.clear();
}