// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "MqttHandleInverter.h"
#include <MessageOutput.h>
#include "MqttSettings.h"
#include "InverterHandler.h"
#include <ctime>
#include "inverters/InverterAbstract.h"

#ifdef HOYMILES
#include <inverters/InverterAbstract.h>
#endif

#define PUBLISH_MAX_INTERVAL 60000

MqttHandleInverterClass MqttHandleInverter;

MqttHandleInverterClass::MqttHandleInverterClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandleInverterClass::loop, this))
{
}

void MqttHandleInverterClass::init(Scheduler& scheduler)
{
    subscribeTopics();

    scheduler.addTask(_loopTask);
    _loopTask.setInterval(Configuration.get().Mqtt.PublishInterval * TASK_SECOND);
    _loopTask.enable();
}

void MqttHandleInverterClass::loop()
{
    _loopTask.setInterval(Configuration.get().Mqtt.PublishInterval * TASK_SECOND);

    if (!MqttSettings.getConnected() || !InverterHandler.isAllRadioIdle()) {
        _loopTask.forceNextIteration();
        return;
    }

    // Loop all inverters
    for (uint8_t i = 0; i < InverterHandler.getNumInverters(); i++) {
        auto inv = InverterHandler.getInverterByPos(i);

        const String subtopic = inv->serialString();

        // Name
        MqttSettings.publish(subtopic + "/name", inv->name());

        if(inv->getInverterType() == inverter_type::Inverter_Hoymiles) {
            auto hoy = (InverterAbstract*)inv.get();
            // Radio Statistics
            MqttSettings.publish(subtopic + "/radio/tx_request", String(hoy->RadioStats.TxRequestData));
            MqttSettings.publish(subtopic + "/radio/tx_re_request", String(hoy->RadioStats.TxReRequestFragment));
            MqttSettings.publish(subtopic + "/radio/rx_success", String(hoy->RadioStats.RxSuccess));
            MqttSettings.publish(subtopic + "/radio/rx_fail_nothing", String(hoy->RadioStats.RxFailNoAnswer));
            MqttSettings.publish(subtopic + "/radio/rx_fail_partial", String(hoy->RadioStats.RxFailPartialAnswer));
            MqttSettings.publish(subtopic + "/radio/rx_fail_corrupt", String(hoy->RadioStats.RxFailCorruptData));
            MqttSettings.publish(subtopic + "/radio/rssi", String(hoy->getLastRssi()));
        }

        if (inv->getDevInfo()->getLastUpdate() > 0) {
            // Bootloader Version
            MqttSettings.publish(subtopic + "/device/bootloaderversion", String(inv->getDevInfo()->getFwBootloaderVersion()));

            // Firmware Version
            MqttSettings.publish(subtopic + "/device/fwbuildversion", String(inv->getDevInfo()->getFwBuildVersion()));

            // Firmware Build DateTime
            MqttSettings.publish(subtopic + "/device/fwbuilddatetime", inv->getDevInfo()->getFwBuildDateTimeStr());

            // Hardware part number
            MqttSettings.publish(subtopic + "/device/hwpartnumber", String(inv->getDevInfo()->getHwPartNumber()));

            // Hardware version
            MqttSettings.publish(subtopic + "/device/hwversion", inv->getDevInfo()->getHwVersion());
        }

        if (inv->getSystemConfigParaParser()->getLastUpdate() > 0) {
            // Limit
            MqttSettings.publish(subtopic + "/status/limit_relative", String(inv->getSystemConfigParaParser()->getLimitPercent()));

            uint16_t maxpower = inv->getDevInfo()->getMaxPower();
            if (maxpower > 0) {
                MqttSettings.publish(subtopic + "/status/limit_absolute", String(inv->getSystemConfigParaParser()->getLimitPercent() * maxpower / 100));
            }
        }

        MqttSettings.publish(subtopic + "/status/reachable", String(inv->isReachable()));
        MqttSettings.publish(subtopic + "/status/producing", String(inv->isProducing()));

        if (inv->getStatistics()->getLastUpdate() > 0) {
            MqttSettings.publish(subtopic + "/status/last_update", String(std::time(0) - (millis() - inv->getStatistics()->getLastUpdate()) / 1000));
        } else {
            MqttSettings.publish(subtopic + "/status/last_update", String(0));
        }
        
        //TODO find better way for deye chek
        const uint32_t lastUpdateInternal = inv->getStatistics()->getLastUpdateFromInternal();
        if (inv->getStatistics()->getLastUpdate() > 0 && ((inv->getInverterType() == Inverter_DeyeSun && inv->isReachable()) || lastUpdateInternal != _lastPublishStats[i])) {
            _lastPublishStats[i] = lastUpdateInternal;

            // Loop all channels
            for (auto& t : inv->getStatistics()->getChannelTypes()) {
                for (auto& c : inv->getStatistics()->getChannelsByType(t)) {
                    if (t == TYPE_DC) {
                        INVERTER_CONFIG_T* inv_cfg = Configuration.getInverterConfig(inv->serial());
                        if (inv_cfg != nullptr) {
                            // TODO(tbnobody)
                            MqttSettings.publish(inv->serialString() + "/" + String(static_cast<uint8_t>(c) + 1) + "/name", inv_cfg->channel[c].Name);
                        }
                    }
                    for (uint8_t f = 0; f < sizeof(_publishFields) / sizeof(FieldId_t); f++) {
                        publishField(inv, t, c, _publishFields[f]);
                    }
                }
            }
        }

        yield();
    }
}

void MqttHandleInverterClass::publishField(std::shared_ptr<BaseInverterClass> inv, const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId)
{
    const String topic = getTopic(inv, type, channel, fieldId);
    if (topic == "") {
        return;
    }

    MqttSettings.publish(topic, inv->getStatistics()->getChannelFieldValueString(type, channel, fieldId));
}

String MqttHandleInverterClass::getTopic(std::shared_ptr<BaseInverterClass> inv, const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId)
{
    if (!inv->getStatistics()->hasChannelFieldValue(type, channel, fieldId)) {
        return "";
    }

    String chanName;
    if (type == TYPE_INV && fieldId == FLD_PDC) {
        chanName = "powerdc";
    } else {
        chanName = inv->getStatistics()->getChannelFieldName(type, channel, fieldId);
        chanName.toLowerCase();
    }

    String chanNum;
    if (type == TYPE_DC) {
        // TODO(tbnobody)
        chanNum = static_cast<uint8_t>(channel) + 1;
    } else {
        chanNum = channel;
    }

    return inv->serialString() + "/" + chanNum + "/" + chanName;
}

void MqttHandleInverterClass::onMqttMessage(Topic t, const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, const size_t len, const size_t index, const size_t total)
{
    const CONFIG_T& config = Configuration.get();

    char token_topic[MQTT_MAX_TOPIC_STRLEN + 40]; // respect all subtopics
    strncpy(token_topic, topic, MQTT_MAX_TOPIC_STRLEN + 40); // convert const char* to char*

    char* serial_str;
    char* rest = &token_topic[strlen(config.Mqtt.Topic)];

    serial_str = strtok_r(rest, "/", &rest);

    if (serial_str == NULL) {
        return;
    }

    const uint64_t serial = strtoull(serial_str, 0, 16);

    auto inv = InverterHandler.getInverterBySerial(serial);

    if (inv == nullptr) {
        MessageOutput.println("Inverter not found");
        return;
    }

    std::string strValue(reinterpret_cast<const char*>(payload), len);
    float payload_val = -1;
    try {
        payload_val = std::stof(strValue);
    } catch (std::invalid_argument const& e) {
        MessageOutput.printf("MQTT handler: cannot parse payload of topic '%s' as float: %s\r\n",
            topic, strValue.c_str());
        return;
    }

    switch (t) {
    case Topic::LimitPersistentRelative:
        // Set inverter limit relative persistent
        MessageOutput.printf("Limit Persistent: %.1f %%\r\n", payload_val);
        inv->sendActivePowerControlRequest(payload_val, PowerLimitControlType::RelativPersistent);
        break;

    case Topic::LimitPersistentAbsolute:
        // Set inverter limit absolute persistent
        MessageOutput.printf("Limit Persistent: %.1f W\r\n", payload_val);
        inv->sendActivePowerControlRequest(payload_val, PowerLimitControlType::AbsolutPersistent);
        break;

    case Topic::LimitNonPersistentRelative:
        // Set inverter limit relative non persistent
        MessageOutput.printf("Limit Non-Persistent: %.1f %%\r\n", payload_val);
        if (!properties.retain) {
            inv->sendActivePowerControlRequest(payload_val, PowerLimitControlType::RelativNonPersistent);
        } else {
            MessageOutput.println("Ignored because retained");
        }
        break;

    case Topic::LimitNonPersistentAbsolute:
        // Set inverter limit absolute non persistent
        MessageOutput.printf("Limit Non-Persistent: %.1f W\r\n", payload_val);
        if (!properties.retain) {
            inv->sendActivePowerControlRequest(payload_val, PowerLimitControlType::AbsolutNonPersistent);
        } else {
            MessageOutput.println("Ignored because retained");
        }
        break;

    case Topic::Power:
        // Turn inverter on or off
        MessageOutput.printf("Set inverter power to: %" PRId32 "\r\n", static_cast<int32_t>(payload_val));
        inv->sendPowerControlRequest(static_cast<int32_t>(payload_val) > 0);
        break;

    case Topic::Restart:
        // Restart inverter
        MessageOutput.printf("Restart inverter\r\n");
        if (!properties.retain && payload_val == 1) {
            inv->sendRestartControlRequest();
        } else {
            MessageOutput.println("Ignored because retained or numeric value not '1'");
        }
        break;

    case Topic::ResetRfStats:
        // Reset RF Stats
        MessageOutput.printf("Reset RF stats\r\n");
        if (!properties.retain && payload_val == 1) {
            inv.get()->resetStats();
        } else {
            MessageOutput.println("Ignored because retained or numeric value not '1'");
        }
    }
}

void MqttHandleInverterClass::subscribeTopics()
{
    String const& prefix = MqttSettings.getPrefix();

    auto subscribe = [&prefix, this](char const* subTopic, Topic t) {
        String fullTopic(prefix + _cmdtopic.data() + subTopic);
        MqttSettings.subscribe(fullTopic.c_str(), 0,
            std::bind(&MqttHandleInverterClass::onMqttMessage, this, t,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6));
    };

    for (auto const& s : _subscriptions) {
    
        subscribe(s.first.data(), s.second);
    }
}

void MqttHandleInverterClass::unsubscribeTopics()
{
    String const& prefix = MqttSettings.getPrefix() + _cmdtopic.data();
    for (auto const& s : _subscriptions) {
        MqttSettings.unsubscribe(prefix + s.first.data());
    }
}
