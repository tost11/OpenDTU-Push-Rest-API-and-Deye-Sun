#include "HiFlowInverter.h"
#include <cstring>
#include <Arduino.h>
#include <esp_log.h>

#undef TAG
static const char* TAG = "HiFlowInv";

HiFlowInverter::HiFlowInverter(uint64_t serial)
{
    _serial = serial;

    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             ((uint32_t)((serial >> 32) & 0xFFFFFFFF)),
             ((uint32_t)(serial & 0xFFFFFFFF)));
    _serialString = serial_buff;

    _alarmLogParser.reset(new DefaultAlarmLog("HiFlowBLE"));
    _devInfoParser.reset(new HiFlowDevInfo());
    _powerCommandParser.reset(new HiFlowPowerCommand());
    _statisticsParser.reset(new HiFlowStatisticsParser());

    _pollTimer.set(15 * 1000); // Default 15s poll interval
    _pollTimer.zero();
}

void HiFlowInverter::setupBle(const char* pin)
{
    // Derive SN from the serial number (hex representation used for BLE name matching and V0 pairing)
    char snHex[13] = {};
    snprintf(snHex, sizeof(snHex), "%04X%08X",
             (uint32_t)((_serial >> 32) & 0xFFFF),
             (uint32_t)(_serial & 0xFFFFFFFF));
    _bleInterface.setup(snHex, pin);
}

void HiFlowInverter::setEncRand(const uint8_t encRand[16])
{
    _bleInterface.setEncRand(encRand);
}

const uint8_t* HiFlowInverter::getEncRand() const
{
    return _bleInterface.getEncRand();
}

bool HiFlowInverter::hasEncRand() const
{
    return _bleInterface.hasEncRand();
}

void HiFlowInverter::startConnection()
{
    _bleInterface.setEnabled(true);
}

void HiFlowInverter::setPollTime(uint16_t pollTime)
{
    _pollTime = pollTime;
    if (_pollTime < 10) _pollTime = 10; // BLE minimum
    _pollTimer.setTimeout(_pollTime * 1000);
}

void HiFlowInverter::onPollTimeChanged()
{
    _pollTimer.setTimeout(_pollTime * 1000);
}

void HiFlowInverter::update()
{
    // Drive BLE state machine
    _bleInterface.loop();

    // Request data when poll timer fires and interface is ready
    if (_enablePolling && _bleInterface.isReady() && _pollTimer.occured()) {
        if (_bleInterface.requestDataUpdate()) {
            _pollTimer.reset();
        }
    }

    // Process received data
    if (_bleInterface.hasNewData()) {
        HiFlowRealData data = _bleInterface.getLatestData();
        if (data.valid) {
            processNewData(data);
            _statisticsParser->resetRxFailureCount();
        }
    }

    // Track connection failures
    if (_bleInterface.getState() == HiFlowBLEState::Error) {
        _statisticsParser->incrementRxFailureCount();
    }
}

void HiFlowInverter::processNewData(const HiFlowRealData& data)
{
    // Fill the packed HiFlowData struct and push to statistics parser
    HiFlowData hfData = {};

    // AC side
    hfData.acCurrent = data.acCurrent;
    hfData.acVoltage = data.acVoltage;
    hfData.acPower = data.acPower;
    hfData.frequency = data.acFrequency;
    hfData.temperature = data.temperature;
    hfData.reactivePower = data.acReactivePower;
    hfData.powerFactor = data.acPowerFactor;

    // Compute daily/total energy from PV data sum
    uint32_t totalDaily = 0;
    uint32_t totalTotal = 0;

    // PV side
    for (uint8_t i = 0; i < data.pvCount && i < 4; i++) {
        hfData.pv[i].voltage = data.pv[i].voltage;
        hfData.pv[i].current = data.pv[i].current;
        hfData.pv[i].power = data.pv[i].power;
        hfData.pv[i].energyDaily = data.pv[i].energyDaily;
        hfData.pv[i].energyTotal = data.pv[i].energyTotal;
        totalDaily += data.pv[i].energyDaily;
        totalTotal += data.pv[i].energyTotal;
    }

    hfData.acEnergyDaily = totalDaily;
    hfData.acEnergyTotal = totalTotal;

    // Push to statistics parser
    _statisticsParser->beginAppendFragment();
    _statisticsParser->clearBuffer();
    _statisticsParser->appendFragment(0, (const uint8_t*)&hfData, sizeof(HiFlowData));
    _statisticsParser->setLastUpdate(millis());
    _statisticsParser->endAppendFragment();

    _devInfoParser->setLastUpdate(millis());

    ESP_LOGI(TAG, "Data updated: AC %d.%dW, %d PV ports",
             data.acPower / 10, data.acPower % 10, data.pvCount);
}

uint64_t HiFlowInverter::serial() const
{
    return _serial;
}

String HiFlowInverter::typeName() const
{
    return _devInfoParser->getHwModelName();
}

bool HiFlowInverter::isProducing()
{
    auto stats = getStatistics();
    float totalAc = 0;
    for (auto& c : stats->getChannelsByType(TYPE_AC)) {
        if (stats->hasChannelFieldValue(TYPE_AC, c, FLD_PAC)) {
            totalAc += stats->getChannelFieldValue(TYPE_AC, c, FLD_PAC);
        }
    }
    return _enablePolling && totalAc > 0;
}

bool HiFlowInverter::isReachable()
{
    return _bleInterface.isConnected();
}

bool HiFlowInverter::sendActivePowerControlRequest(float limit, PowerLimitControlType type)
{
    // Not implemented in prototype
    (void)limit;
    (void)type;
    return false;
}

bool HiFlowInverter::resendPowerControlRequest()
{
    return false;
}

bool HiFlowInverter::sendRestartControlRequest()
{
    return false;
}

bool HiFlowInverter::sendPowerControlRequest(bool turnOn)
{
    (void)turnOn;
    return false;
}

inverter_type HiFlowInverter::getInverterType() const
{
    return inverter_type::Inverter_HiFlowBLE;
}

void HiFlowInverter::resetStats()
{
}

bool HiFlowInverter::supportsPowerDistributionLogic()
{
    return false;
}
