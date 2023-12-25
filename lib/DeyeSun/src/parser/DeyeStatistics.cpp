#include "DeyeStatistics.h"

DeyeStatistics::DeyeStatistics() {
    value = "Value";
    unit = "Unit";
    field = "Field";
}

std::list<ChannelNum_t> DeyeStatistics::getChannelsByType(ChannelType_t type) const {
    //TODO implmeent

    std::list<ChannelNum_t> ret;

    if(type == ChannelType_t::TYPE_INV){
        ret.push_back(ChannelNum_t::CH0);
    }else if(type == ChannelType_t::TYPE_AC){
        ret.push_back(ChannelNum_t::CH0);
    }else if(type == ChannelType_t::TYPE_DC){
        ret.push_back(ChannelNum_t::CH0);
        ret.push_back(ChannelNum_t::CH1);
    }

    return ret;
}

float DeyeStatistics::getChannelFieldValue(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) {
    return 1;
}

uint8_t DeyeStatistics::getChannelFieldDigits(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const {
    return 1;
}

bool DeyeStatistics::hasChannelFieldValue(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const {
    return true;
}

const char *DeyeStatistics::getChannelFieldName(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const {
    return value.c_str();
}

String DeyeStatistics::getChannelFieldValueString(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) {
    return field;
}

const char *DeyeStatistics::getChannelFieldUnit(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) const {
    return unit.c_str();
}
