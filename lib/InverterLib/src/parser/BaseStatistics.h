#pragma once

#include <list>
#include <cstdint>
#include <WString.h>
#include "Updater.h"

enum ChannelType_t {
    TYPE_AC = 0,
    TYPE_DC,
    TYPE_INV
};

// CH0 is default channel (freq, ac, temp)
enum ChannelNum_t {
    CH0 = 0,
    CH1,
    CH2,
    CH3,
    CH4,
    CH5,
    CH_CNT
};

// field types
enum FieldId_t {
    FLD_UDC = 0,
    FLD_IDC,
    FLD_PDC,
    FLD_YD,
    FLD_YT,
    FLD_UAC,
    FLD_IAC,
    FLD_PAC,
    FLD_F,
    FLD_T,
    FLD_PF,
    FLD_EFF,
    FLD_IRR,
    FLD_Q,
    FLD_EVT_LOG,
    // HMT only
    FLD_UAC_1N,
    FLD_UAC_2N,
    FLD_UAC_3N,
    FLD_UAC_12,
    FLD_UAC_23,
    FLD_UAC_31,
    FLD_IAC_1,
    FLD_IAC_2,
    FLD_IAC_3
};

const char* const channelsTypes[] = { "AC", "DC", "INV" };

class BaseStatistics : public Updater {

public:
    std::list<ChannelType_t> getChannelTypes() const;
    const char* getChannelTypeName(const ChannelType_t type) const;
    virtual std::list<ChannelNum_t> getChannelsByType(const ChannelType_t type) const = 0;

    virtual float getChannelFieldValue(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) = 0;
    virtual uint8_t getChannelFieldDigits(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const = 0;
    virtual bool hasChannelFieldValue(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const = 0;
    virtual const char* getChannelFieldName(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const = 0;
    virtual String getChannelFieldValueString(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) = 0;
    virtual const char* getChannelFieldUnit(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const = 0;
    virtual void setChannelFieldOffset(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId, float offset,uint8_t index = 0) = 0;
    virtual void zeroDailyData() = 0;
    virtual void resetYieldDayCorrection() = 0;
    virtual void setYieldDayCorrection(const bool enabled) = 0;
    virtual void setDeyeSunOfflineYieldDayCorrection(const bool enabled) = 0;

    uint16_t getStringMaxPower(const uint8_t channel) const;
    void setStringMaxPower(const uint8_t channel, const uint16_t power);

    // Update time when internal data structure changes (from inverter and by internal manipulation)
    uint32_t getLastUpdateFromInternal() const;
    void setLastUpdateFromInternal(uint32_t lastUpdate);
private:

    uint16_t _stringMaxPower[CH_CNT];
    uint32_t _lastUpdateFromInternal = 0;
};
