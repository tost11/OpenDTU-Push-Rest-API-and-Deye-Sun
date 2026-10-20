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

    // Consume control command result
    bool commandOk = false;
    if (_cmdInFlight && _bleInterface.getCommandResult(commandOk)) {
        _cmdInFlight = false;
        LastCommandSuccess result = commandOk ? CMD_OK : CMD_NOK;
        if (_cmdInFlightType == PendingCmd::Limit) {
            getSystemConfigParaParser()->setLastLimitCommandSuccess(result);
            if (commandOk) {
                getSystemConfigParaParser()->setLimitPercent(_limitLevel / 10.0f);
                _needGetConfig = false;
            }
        } else {
            getPowerCommand()->setLastPowerCommandSuccess(result);
        }
    }

    // Consume GetConfig result (power limit read)
    int16_t configLimitLevel = 0;
    if (_bleInterface.getConfigResult(configLimitLevel)) {
        _configRequestInFlight = false;
        getSystemConfigParaParser()->setLimitPercent(configLimitLevel > 0 ? configLimitLevel / 10.0f : 100.0f);
        getSystemConfigParaParser()->setLastLimitRequestSuccess(CMD_OK);
        _needGetConfig = false;
    }

    // Send queued control command
    if (_cmdQueued && !_cmdInFlight && _bleInterface.isReady()) {
        bool paced = (_lastCmdTime != 0 && millis() - _lastCmdTime < CMD_RETRY_INTERVAL_MS);
        if (!paced) {
            uint8_t action = 0;
            char data[32] = {};
            uint64_t miSn = 0;
            uint8_t devKind = 0;
            bool canSend = true;
            switch (_pendingCmd) {
                case PendingCmd::Limit:
                    action = HiFlowProtocol::ACTION_LIMIT_POWER;
                    snprintf(data, sizeof(data), "A:%u,B:0,C:0\r", _limitLevel);
                    break;
                case PendingCmd::Power:
                    // wait until the inverter serial is known from RealDataNew
                    canSend = (_inverterSerial != 0);
                    action = _powerCmdTurnOn ? HiFlowProtocol::ACTION_MI_START : HiFlowProtocol::ACTION_MI_SHUTDOWN;
                    miSn = _inverterSerial;
                    devKind = HiFlowProtocol::DEV_KIND_DTU;
                    break;
                case PendingCmd::Restart:
                    // wait until the inverter serial is known from RealDataNew
                    canSend = (_inverterSerial != 0);
                    action = HiFlowProtocol::ACTION_MI_REBOOT;
                    miSn = _inverterSerial;
                    devKind = HiFlowProtocol::DEV_KIND_DTU;
                    break;
                case PendingCmd::DtuRestart:
                    action = HiFlowProtocol::ACTION_DTU_REBOOT;
                    break;
                default:
                    _cmdQueued = false;
            }
            if (canSend && _cmdQueued && _bleInterface.sendControlCommand(action, data, miSn, devKind)) {
                _cmdQueued = false;
                _cmdInFlight = true;
                _cmdInFlightType = _pendingCmd;
                _lastCmdTime = millis();
            }
        }
    }

    // Request config when the power limit is unknown
    if (_needGetConfig && !_configRequestInFlight && _bleInterface.isReady()) {
        if (_bleInterface.requestGetConfig()) {
            _configRequestInFlight = true;
        }
    }

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

    // Inverter serial for control commands (mi_to_sn)
    if (data.inverterSerial != 0) {
        _inverterSerial = data.inverterSerial;
    }

    // Power limit is not reliably reported in RealDataNew (SGSMO field 13),
    // read it via GetConfig after each poll
    _needGetConfig = true;

    ESP_LOGI(TAG, "Data updated: AC %d.%dW, %d PV ports, warn=%d, link=%d",
             data.acPower / 10, data.acPower % 10, data.pvCount, data.warningNumber, data.linkStatus);
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
    uint16_t percent;
    if (type == RelativPersistent || type == RelativNonPersistent) {
        percent = (uint16_t)(limit + 0.5);
    } else {
        uint16_t maxPower = _devInfoParser->getMaxPower();
        if (maxPower == 0) {
            _alarmLogParser->addAlarm(6, 10 * 60, "power limit command not sent: no known max power for this inverter");
            getSystemConfigParaParser()->setLastLimitCommandSuccess(CMD_NOK);
            return false;
        }
        percent = (uint16_t)(limit / (float)maxPower * 100);
        if (limit > 0 && percent == 0) {
            percent = 1; // a positive watt value must not become "no limit" (0%)
        }
    }
    if (percent > 100) {
        percent = 100;
    }

    uint16_t limitLevel = percent * 10;
    if (limitLevel > 1000) {
        limitLevel = 1000;
    }

    _limitLevel = limitLevel;
    _pendingCmd = PendingCmd::Limit;
    _cmdQueued = true;
    _lastCmdTime = 0; // explicit request: send as soon as possible
    getSystemConfigParaParser()->setLastLimitCommandSuccess(CMD_PENDING);
    return true;
}

bool HiFlowInverter::resendPowerControlRequest()
{
    // Re-queue the last power control command (on/off, inverter restart, DTU restart)
    if (_pendingCmd != PendingCmd::Power && _pendingCmd != PendingCmd::Restart && _pendingCmd != PendingCmd::DtuRestart) {
        return false;
    }
    // Pace retries
    if (_lastCmdTime != 0 && millis() - _lastCmdTime < CMD_RETRY_INTERVAL_MS) {
        return true;
    }
    _cmdQueued = true;
    getPowerCommand()->setLastPowerCommandSuccess(CMD_PENDING);
    return true;
}

bool HiFlowInverter::resendLimitCommand()
{
    if (_pendingCmd != PendingCmd::Limit) {
        return false;
    }
    // Pace retries
    if (_lastCmdTime != 0 && millis() - _lastCmdTime < CMD_RETRY_INTERVAL_MS) {
        return true;
    }
    _cmdQueued = true;
    getSystemConfigParaParser()->setLastLimitCommandSuccess(CMD_PENDING);
    return true;
}

bool HiFlowInverter::sendRestartControlRequest()
{
    _pendingCmd = PendingCmd::Restart;
    _cmdQueued = true;
    _lastCmdTime = 0; // explicit request: send as soon as possible
    getPowerCommand()->setLastPowerCommandSuccess(CMD_PENDING);
    return true;
}

bool HiFlowInverter::sendPowerControlRequest(bool turnOn)
{
    _pendingCmd = PendingCmd::Power;
    _powerCmdTurnOn = turnOn;
    _cmdQueued = true;
    _lastCmdTime = 0; // explicit request: send as soon as possible
    getPowerCommand()->setLastPowerCommandSuccess(CMD_PENDING);
    return true;
}

bool HiFlowInverter::sendDtuRestartRequest()
{
    _pendingCmd = PendingCmd::DtuRestart;
    _cmdQueued = true;
    _lastCmdTime = 0; // explicit request: send as soon as possible
    getPowerCommand()->setLastPowerCommandSuccess(CMD_PENDING);
    return true;
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
