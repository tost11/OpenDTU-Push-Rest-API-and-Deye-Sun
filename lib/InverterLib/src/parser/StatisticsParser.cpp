// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 - 2023 Thomas Basler and others
 */
#include "StatisticsParser.h"
#include <MessageOutput.h>

static float calcTotalYieldTotal(StatisticsParser* iv, uint8_t arg0);
static float calcTotalYieldDay(StatisticsParser* iv, uint8_t arg0);
static float calcChUdc(StatisticsParser* iv, uint8_t arg0);
static float calcTotalPowerDc(StatisticsParser* iv, uint8_t arg0);
static float calcPowerDc(StatisticsParser* iv, uint8_t arg0);
static float calcTotalEffiency(StatisticsParser* iv, uint8_t arg0);
static float calcChIrradiation(StatisticsParser* iv, uint8_t arg0);
static float calcTotalCurrentAc(StatisticsParser* iv, uint8_t arg0);

using func_t = float(StatisticsParser*, uint8_t);

struct calcFunc_t {
    uint8_t funcId; // unique id
    func_t* func; // function pointer
};

const calcFunc_t calcFunctions[] = {
    { CALC_TOTAL_YT, &calcTotalYieldTotal },
    { CALC_TOTAL_YD, &calcTotalYieldDay },
    { CALC_CH_UDC, &calcChUdc },
    { CALC_TOTAL_PDC, &calcTotalPowerDc },
    { CALC_TOTAL_EFF, &calcTotalEffiency },
    { CALC_CH_IRR, &calcChIrradiation },
    { CALC_TOTAL_IAC, &calcTotalCurrentAc },
    { CALC_PDC, &calcPowerDc },
};

const FieldId_t runtimeFields[] = {
    FLD_UDC,
    FLD_IDC,
    FLD_PDC,
    FLD_UAC,
    FLD_IAC,
    FLD_PAC,
    FLD_F,
    FLD_T,
    FLD_PF,
    FLD_Q,
    FLD_UAC_1N,
    FLD_UAC_2N,
    FLD_UAC_3N,
    FLD_UAC_12,
    FLD_UAC_23,
    FLD_UAC_31,
    FLD_IAC_1,
    FLD_IAC_2,
    FLD_IAC_3,
};

const FieldId_t dailyProductionFields[] = {
    FLD_YD,
};

StatisticsParser::StatisticsParser()
    : Parser()
{
}

void StatisticsParser::setByteAssignment(const byteAssign_t* byteAssignment, const uint8_t size)
{
    _byteAssignment = byteAssignment;
    _byteAssignmentSize = size;

    for (uint8_t i = 0; i < _byteAssignmentSize; i++) {
        if (_byteAssignment[i].div == CMD_CALC) {
            continue;
        }
        _expectedByteCount = max<uint8_t>(_expectedByteCount, _byteAssignment[i].start + _byteAssignment[i].num);
    }
}

uint8_t StatisticsParser::getExpectedByteCount()
{
    return _expectedByteCount;
}

void StatisticsParser::clearBuffer()
{
    memset(_payloadStatistic, 0, getStaticPayloadSize());
    _statisticLength = 0;
}

void StatisticsParser::appendFragment(const uint8_t offset, const uint8_t* payload, const uint8_t len)
{
    if (offset + len > getStaticPayloadSize()) {
        MessageOutput.printf("FATAL: (%s, %d) stats packet too large for buffer\r\n", __FILE__, __LINE__);
        //TODO log this again out
        //Hoymiles.getMessageOutput()->printf("FATAL: (%s, %d) stats packet too large for buffer\r\n", __FILE__, __LINE__);
        return;
    }
    memcpy(&_payloadStatistic[offset], payload, len);
    _statisticLength += len;
}

void StatisticsParser::endAppendFragment()
{
    Parser::endAppendFragment();

    if (!_enableYieldDayCorrection) {
        resetYieldDayCorrection();
        return;
    }

    for (auto& c : getChannelsByType(TYPE_DC)) {
        // check if current yield day is smaller then last cached yield day
        if (getChannelFieldValue(TYPE_DC, c, FLD_YD) < _lastYieldDay[static_cast<uint8_t>(c)]) {
            // currently all values are zero --> Add last known values to offset
            //TODO log this out again
            MessageOutput.printf("Yield Day reset detected!\r\n");

            setChannelFieldOffset(TYPE_DC, c, FLD_YD, _lastYieldDay[static_cast<uint8_t>(c)]);

            _lastYieldDay[static_cast<uint8_t>(c)] = 0;
        } else {
            _lastYieldDay[static_cast<uint8_t>(c)] = getChannelFieldValue(TYPE_DC, c, FLD_YD);
        }
    }
}

const byteAssign_t* StatisticsParser::getAssignmentByChannelField(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId) const
{
    for (uint8_t i = 0; i < _byteAssignmentSize; i++) {
        if (_byteAssignment[i].type == type && _byteAssignment[i].ch == channel && _byteAssignment[i].fieldId == fieldId) {
            return &_byteAssignment[i];
        }
    }
    return nullptr;
}

fieldSettings_t* StatisticsParser::getSettingByChannelField(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId, uint8_t index)
{
    for (auto& i : _fieldSettings) {
        if (i.type == type && i.ch == channel && i.fieldId == fieldId && i.index == index) {
            return &i;
        }
    }
    return nullptr;
}

float StatisticsParser::getChannelFieldValue(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId)
{
    const byteAssign_t* pos = getAssignmentByChannelField(type, channel, fieldId);
    if (pos == nullptr) {
        return 0;
    }

    if (CMD_CALC != pos->div) {
        // Value is a static value
        uint32_t val = 0;
        HOY_SEMAPHORE_TAKE();
        if(pos->littleEndian){
            uint8_t ptr = pos->start;
            const uint8_t end = ptr + pos->num;
            do {
                val <<= 8;
                val |= _payloadStatistic[ptr];
            } while (++ptr != end);
        }else{
            uint8_t ptr = pos->start + pos->num;
            const uint8_t end = pos->start;
            do {
                val <<= 8;
                val |= _payloadStatistic[ptr-1];
            } while (--ptr != end);
        }
        HOY_SEMAPHORE_GIVE();

        float result;
        if (pos->isSigned && pos->num == 2) {
            result = static_cast<float>(static_cast<int16_t>(val));
        } else if (pos->isSigned && pos->num == 4) {
            result = static_cast<float>(static_cast<int32_t>(val));
        } else {
            result = static_cast<float>(val);
        }

        result /= static_cast<float>(pos->div);

        if(_statisticLength > 0){
            result += calculateOffsetByChannelField(type, channel, fieldId);
        }
        return result;
    } else {
        // Value has to be calculated
        return calcFunctions[pos->start].func(this, pos->num);
    }

    return 0;
}

bool StatisticsParser::setChannelFieldValue(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId, float value)
{
    const byteAssign_t* pos = getAssignmentByChannelField(type, channel, fieldId);
    if (pos == nullptr) {
        return false;
    }

    uint8_t ptr = pos->start + pos->num - 1;
    const uint8_t end = pos->start;
    const uint16_t div = pos->div;

    if (CMD_CALC == div) {
        return false;
    }

    value += calculateOffsetByChannelField(type, channel, fieldId);

    value *= static_cast<float>(div);

    uint32_t val = 0;
    if (pos->isSigned && pos->num == 2) {
        val = static_cast<uint32_t>(static_cast<int16_t>(value));
    } else if (pos->isSigned && pos->num == 4) {
        val = static_cast<uint32_t>(static_cast<int32_t>(value));
    } else {
        val = static_cast<uint32_t>(value);
    }

    HOY_SEMAPHORE_TAKE();
    do {
        _payloadStatistic[ptr] = val;
        val >>= 8;
        if(ptr == 0){
            break;
        }
    } while (--ptr >= end);
    HOY_SEMAPHORE_GIVE();

    return true;
}

String StatisticsParser::getChannelFieldValueString(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId)
{
    return String(
        getChannelFieldValue(type, channel, fieldId),
        static_cast<unsigned int>(getChannelFieldDigits(type, channel, fieldId)));
}

bool StatisticsParser::hasChannelFieldValue(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId) const
{
    const byteAssign_t* pos = getAssignmentByChannelField(type, channel, fieldId);
    return pos != nullptr;
}

const char* StatisticsParser::getChannelFieldUnit(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId) const
{
    const byteAssign_t* pos = getAssignmentByChannelField(type, channel, fieldId);
    return units[pos->unitId];
}

UnitId_t StatisticsParser::getChannelFieldUnitId(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId) const
{
    const byteAssign_t* pos = getAssignmentByChannelField(type, channel, fieldId);
    return pos->unitId;
}

const char* StatisticsParser::getChannelFieldName(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId) const
{
    const byteAssign_t* pos = getAssignmentByChannelField(type, channel, fieldId);
    return fields[pos->fieldId];
}

uint8_t StatisticsParser::getChannelFieldDigits(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId) const
{
    const byteAssign_t* pos = getAssignmentByChannelField(type, channel, fieldId);
    return pos->digits;
}

float StatisticsParser::getChannelFieldOffset(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId, uint8_t index)
{
    const fieldSettings_t* setting = getSettingByChannelField(type, channel, fieldId,index);
    if (setting != nullptr) {
        return setting->offset;
    }
    return 0;
}

void StatisticsParser::setChannelFieldOffset(const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId, const float offset,uint8_t index)
{
    fieldSettings_t* setting = getSettingByChannelField(type, channel, fieldId,index);
    if (setting != nullptr) {
        setting->offset = offset;
    } else {
        _fieldSettings.push_back({ type, channel, fieldId , index, offset});
    }
}

std::list<ChannelNum_t> StatisticsParser::getChannelsByType(const ChannelType_t type) const
{
    std::list<ChannelNum_t> l;
    for (uint8_t i = 0; i < _byteAssignmentSize; i++) {
        if (_byteAssignment[i].type == type) {
            l.push_back(_byteAssignment[i].ch);
        }
    }
    l.unique();
    return l;
}


void StatisticsParser::resetRxFailureCount()
{
    _rxFailureCount = 0;
}

void StatisticsParser::incrementRxFailureCount()
{
    _rxFailureCount++;
}

uint32_t StatisticsParser::getRxFailureCount() const
{
    return _rxFailureCount;
}

void StatisticsParser::zeroRuntimeData()
{
    zeroFields(runtimeFields);
}

void StatisticsParser::zeroDailyData()
{
    zeroFields(dailyProductionFields);
}

void StatisticsParser::setLastUpdate(const uint32_t lastUpdate)
{
    Updater::setLastUpdate(lastUpdate);
    setLastUpdateFromInternal(lastUpdate);
}


bool StatisticsParser::getYieldDayCorrection() const
{
    return _enableYieldDayCorrection;
}

void StatisticsParser::setYieldDayCorrection(const bool enabled)
{
    _enableYieldDayCorrection = enabled;
}

void StatisticsParser::setDeyeSunOfflineYieldDayCorrection(const bool enabled)
{
    _enableDeyeSunOfflineYieldDayCorrection = enabled;
}

void StatisticsParser::zeroFields(const FieldId_t* fields)
{
    // Loop all channels
    for (auto& t : getChannelTypes()) {
        for (auto& c : getChannelsByType(t)) {
            for (uint8_t i = 0; i < (sizeof(runtimeFields) / sizeof(runtimeFields[0])); i++) {
                if (hasChannelFieldValue(t, c, fields[i])) {
                    setChannelFieldValue(t, c, fields[i], 0);
                }
            }
        }
    }
    setLastUpdateFromInternal(millis());
}

void StatisticsParser::resetYieldDayCorrection()
{
    // new day detected, reset counters
    for (auto& c : getChannelsByType(TYPE_DC)) {
        setChannelFieldOffset(TYPE_DC, c, FLD_YD, 0);
        _lastYieldDay[static_cast<uint8_t>(c)] = 0;
    }
}

float StatisticsParser::calculateOffsetByChannelField(const ChannelType_t type, const ChannelNum_t channel,
                                                                 const FieldId_t fieldId) {
    float ret = 0.f;
    for (auto& i : _fieldSettings) {
        if (i.type == type && i.ch == channel && i.fieldId == fieldId) {
            ret += i.offset;
        }
    }
    return ret;
}

bool StatisticsParser::getDeyeSunOfflineYieldDayCorrection() const {
    return _enableDeyeSunOfflineYieldDayCorrection;
}

void StatisticsParser::resetDeyeSunOfflineYieldDayCorrection(bool setZero) {
    //zero deye sun daily offset on Gateway (ap connection) mode
    for (auto& t : getChannelTypes()) {
        for (auto &c: getChannelsByType(t)) {
            if(!hasChannelFieldValue(t,c,FLD_YD)){
                continue;
            }
            auto setting = getSettingByChannelField(t,c,FLD_YD,1);
            if(setting != nullptr){
                //get real value without our extra offsets (first current, second if reset at night)
                if(!setZero){
                    //if not reset data on midnight move current offset to index 2 so 1 can be recognised as not used
                    float currentOffset = getChannelFieldOffset(t,c,FLD_YD,1);
                    if(currentOffset != 0){
                        //only do if present (if not persent keep last value)
                        setChannelFieldOffset(t,c,FLD_YD,currentOffset,2);
                        MessageOutput.printf("Daily Reset Deye Offline offset to: %f\n",currentOffset);
                    }else{
                        MessageOutput.printf("Daily Reset kept last offset value: %f\n",getChannelFieldOffset(t,c,FLD_YD,2));
                    }
                }else{
                    //if reset data on midnight set everything to zero
                    setChannelFieldOffset(t,c,FLD_YD,0,2);
                    MessageOutput.printf("Daily Reset Deye Offline offset to zero");
                }
                setting->offset = 0.f;
            }
        }
    }
}

static float calcTotalYieldTotal(StatisticsParser* iv, uint8_t arg0)
{
    float yield = 0;
    for (auto& channel : iv->getChannelsByType(TYPE_DC)) {
        yield += iv->getChannelFieldValue(TYPE_DC, channel, FLD_YT);
    }
    return yield;
}

static float calcTotalYieldDay(StatisticsParser* iv, uint8_t arg0)
{
    float yield = 0;
    for (auto& channel : iv->getChannelsByType(TYPE_DC)) {
        yield += iv->getChannelFieldValue(TYPE_DC, channel, FLD_YD);
    }
    return yield;
}

// arg0 = channel of source
static float calcChUdc(StatisticsParser* iv, uint8_t arg0)
{
    return iv->getChannelFieldValue(TYPE_DC, static_cast<ChannelNum_t>(arg0), FLD_UDC);
}

static float calcTotalPowerDc(StatisticsParser* iv, uint8_t arg0)
{
    float dcPower = 0;
    for (auto& channel : iv->getChannelsByType(TYPE_DC)) {
        dcPower += iv->getChannelFieldValue(TYPE_DC, channel, FLD_PDC);
    }
    return dcPower;
}

static float calcPowerDc(StatisticsParser* iv, uint8_t arg0)
{
    return iv->getChannelFieldValue(TYPE_DC,(ChannelNum_t)arg0,FLD_UDC) * iv->getChannelFieldValue(TYPE_DC,(ChannelNum_t)arg0,FLD_IDC);
}


// arg0 = channel
static float calcTotalEffiency(StatisticsParser* iv, uint8_t arg0)
{
    float acPower = 0;
    for (auto& channel : iv->getChannelsByType(TYPE_AC)) {
        acPower += iv->getChannelFieldValue(TYPE_AC, channel, FLD_PAC);
    }

    float dcPower = 0;
    for (auto& channel : iv->getChannelsByType(TYPE_DC)) {
        dcPower += iv->getChannelFieldValue(TYPE_DC, channel, FLD_PDC);
    }

    if (dcPower > 0) {
        return acPower / dcPower * 100.0f;
    }
    return 0.0;
}

// arg0 = channel
static float calcChIrradiation(StatisticsParser* iv, uint8_t arg0)
{
    if (nullptr != iv) {
        if (iv->getStringMaxPower(arg0) > 0)
            return iv->getChannelFieldValue(TYPE_DC, static_cast<ChannelNum_t>(arg0), FLD_PDC) / iv->getStringMaxPower(arg0) * 100.0f;
    }
    return 0.0;
}

static float calcTotalCurrentAc(StatisticsParser* iv, uint8_t arg0)
{
    float acCurrent = 0;
    acCurrent += iv->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_1);
    acCurrent += iv->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_2);
    acCurrent += iv->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_3);
    return acCurrent;
}
