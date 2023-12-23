// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Parser.h"
#include "BaseStatistics.h"
#include <cstdint>
#include <list>

#define STATISTIC_PACKET_SIZE (7 * 16)

// units
enum UnitId_t {
    UNIT_V = 0,
    UNIT_A,
    UNIT_W,
    UNIT_WH,
    UNIT_KWH,
    UNIT_HZ,
    UNIT_C,
    UNIT_PCT,
    UNIT_VAR,
    UNIT_NONE
};
const char* const units[] = { "V", "A", "W", "Wh", "kWh", "Hz", "°C", "%", "var", "" };

const char* const fields[] = { "Voltage", "Current", "Power", "YieldDay", "YieldTotal",
    "Voltage", "Current", "Power", "Frequency", "Temperature", "PowerFactor", "Efficiency", "Irradiation", "ReactivePower", "EventLogCount",
    "Voltage Ph1-N", "Voltage Ph2-N", "Voltage Ph3-N", "Voltage Ph1-Ph2", "Voltage Ph2-Ph3", "Voltage Ph3-Ph1", "Current Ph1", "Current Ph2", "Current Ph3" };

// indices to calculation functions, defined in hmInverter.h
enum {
    CALC_YT_CH0 = 0,
    CALC_YD_CH0,
    CALC_UDC_CH,
    CALC_PDC_CH0,
    CALC_EFF_CH0,
    CALC_IRR_CH,
    CALC_PDC,
};
enum { CMD_CALC = 0xffff };

typedef struct {
    ChannelType_t type;
    ChannelNum_t ch; // channel 0 - 5
    FieldId_t fieldId; // field id
    UnitId_t unitId; // uint id
    uint8_t start; // pos of first byte in buffer
    uint8_t num; // number of bytes in buffer
    uint16_t div; // divisor / calc command
    bool isSigned; // allow negative numbers
    uint8_t digits; // number of valid digits after the decimal point
} byteAssign_t;

typedef struct {
    ChannelType_t type;
    ChannelNum_t ch; // channel 0 - 5
    FieldId_t fieldId; // field id
    float offset; // offset (positive/negative) to be applied on the fetched value
} fieldSettings_t;

class StatisticsParser : public Parser, public BaseStatistics {
public:
    StatisticsParser();
    void clearBuffer();
    void appendFragment(uint8_t offset, uint8_t* payload, uint8_t len);

    void setByteAssignment(const byteAssign_t* byteAssignment, uint8_t size);

    // Returns 1 based amount of expected bytes of statistic data
    uint8_t getExpectedByteCount();

    const byteAssign_t* getAssignmentByChannelField(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId);
    fieldSettings_t* getSettingByChannelField(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId);

    float getChannelFieldValue(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;
    String getChannelFieldValueString(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;
    bool hasChannelFieldValue(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;
    const char* getChannelFieldUnit(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;
    const char* getChannelFieldName(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;
    uint8_t getChannelFieldDigits(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId) override;

    bool setChannelFieldValue(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId, float value);

    float getChannelFieldOffset(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId);
    void setChannelFieldOffset(ChannelType_t type, ChannelNum_t channel, FieldId_t fieldId, float offset) override;

    std::list<ChannelNum_t> getChannelsByType(ChannelType_t type) override;

    void resetRxFailureCount();
    void incrementRxFailureCount();
    uint32_t getRxFailureCount();

    void zeroRuntimeData();
    void zeroDailyData();

    // Update time when new data from the inverter is received
    void setLastUpdate(uint32_t lastUpdate);


private:
    void zeroFields(const FieldId_t* fields);

    uint8_t _payloadStatistic[STATISTIC_PACKET_SIZE] = {};
    uint8_t _statisticLength = 0;

    const byteAssign_t* _byteAssignment;
    uint8_t _byteAssignmentSize;
    uint8_t _expectedByteCount = 0;
    std::list<fieldSettings_t> _fieldSettings;

    uint32_t _rxFailureCount = 0;
};