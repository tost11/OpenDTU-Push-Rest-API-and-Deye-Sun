#pragma once

#include "BaseStatistics.h"

class DeyeStatistics : public BaseStatistics {

public:
    DeyeStatistics();

    std::list<ChannelNum_t> getChannelsByType(ChannelType_t type) override;

    float getChannelFieldValue(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;

    uint8_t getChannelFieldDigits(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;

    bool hasChannelFieldValue(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;

    const char *getChannelFieldName(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;

    String getChannelFieldValueString(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;

    const char *getChannelFieldUnit(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;

private:
    String unit;
    String value;
    String field;
};
