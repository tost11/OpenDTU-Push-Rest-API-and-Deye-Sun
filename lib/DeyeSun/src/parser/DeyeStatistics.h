#pragma once

#include "BaseStatistics.h"

class DeyeStatistics : public BaseStatistics {

public:
    DeyeStatistics();

    std::list<ChannelNum_t> getChannelsByType(ChannelType_t type) const override;

    float getChannelFieldValue(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;

    uint8_t getChannelFieldDigits(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const override;

    bool hasChannelFieldValue(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const override;

    const char *getChannelFieldName(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const override;

    String getChannelFieldValueString(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;

    const char *getChannelFieldUnit(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const override;

private:
    String unit;
    String value;
    String field;
};
