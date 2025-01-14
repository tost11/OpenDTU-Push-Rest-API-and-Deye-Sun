// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_ws_live.h"
#include "Datastore.h"
#include "MessageOutput.h"
#include "Utils.h"
#include "WebApi.h"
#include "defaults.h"
#include "InverterHandler.h"
#include <AsyncJson.h>

#ifdef HOYMILES
#include <Hoymiles.h>
#endif

WebApiWsLiveClass::WebApiWsLiveClass()
    : _ws("/livedata")
    , _wsCleanupTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::wsCleanupTaskCb, this))
    , _sendDataTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::sendDataTaskCb, this))
{
}

void WebApiWsLiveClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    server.on("/api/livedata/status", HTTP_GET, std::bind(&WebApiWsLiveClass::onLivedataStatus, this, _1));

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    scheduler.addTask(_wsCleanupTask);
    _wsCleanupTask.enable();

    scheduler.addTask(_sendDataTask);
    _sendDataTask.enable();
    _simpleDigestAuth.setUsername(AUTH_USERNAME);
    _simpleDigestAuth.setRealm("live websocket");

    reload();
}

void WebApiWsLiveClass::reload()
{
    _ws.removeMiddleware(&_simpleDigestAuth);

    auto const& config = Configuration.get();

    if (config.Security.AllowReadonly) { return; }

    _ws.enable(false);
    _simpleDigestAuth.setPassword(config.Security.Password);
    _ws.addMiddleware(&_simpleDigestAuth);
    _ws.closeAll();
    _ws.enable(true);
}

void WebApiWsLiveClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}

void WebApiWsLiveClass::sendDataTaskCb()
{
    // do nothing if no WS client is connected
    if (_ws.count() == 0) {
        return;
    }

    uint32_t maxTimeStamp = 0;
    // Loop all inverters
    for (uint8_t i = 0; i < InverterHandler.getNumInverters(); i++) {
        auto inv = InverterHandler.getInverterByPos(i);
        maxTimeStamp = std::max<uint32_t>(maxTimeStamp, inv->getStatistics()->getLastUpdate());

        if (inv == nullptr) {
            continue;
        }

        const uint32_t lastUpdateInternal = inv->getStatistics()->getLastUpdateFromInternal();
        if (!((lastUpdateInternal > 0 && lastUpdateInternal > _lastPublishStats[i]) || (millis() - _lastPublishStats[i] > (10 * 1000)))) {
            continue;
        }

        _lastPublishStats[i] = millis();

        try {
            std::lock_guard<std::mutex> lock(_mutex);
            JsonDocument root;
            JsonVariant var = root;

            auto invArray = var["inverters"].to<JsonArray>();
            auto invObject = invArray.add<JsonObject>();

            generateCommonJsonResponse(var);
            generateInverterCommonJsonResponse(invObject, inv);
            generateInverterChannelJsonResponse(invObject, inv);

            if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
                continue;
            }

            String buffer;
            serializeJson(root, buffer);

            _ws.textAll(buffer);

        } catch (const std::bad_alloc& bad_alloc) {
            MessageOutput.printf("Call to /api/livedata/status temporarely out of resources. Reason: \"%s\".\r\n", bad_alloc.what());
        } catch (const std::exception& exc) {
            MessageOutput.printf("Unknown exception in /api/livedata/status. Reason: \"%s\".\r\n", exc.what());
        }
    }
}

void WebApiWsLiveClass::generateCommonJsonResponse(JsonVariant& root)
{
    auto totalObj = root["total"].to<JsonObject>();
    addTotalField(totalObj, "Power", Datastore.getTotalAcPowerEnabled(), "W", Datastore.getTotalAcPowerDigits());
    addTotalField(totalObj, "YieldDay", Datastore.getTotalAcYieldDayEnabled(), "Wh", Datastore.getTotalAcYieldDayDigits());
    addTotalField(totalObj, "YieldTotal", Datastore.getTotalAcYieldTotalEnabled(), "kWh", Datastore.getTotalAcYieldTotalDigits());

    JsonObject hintObj = root["hints"].to<JsonObject>();
    struct tm timeinfo;
    hintObj["time_sync"] = !getLocalTime(&timeinfo, 5);
    #ifdef HOYMILES
    hintObj["radio_problem"] = (Hoymiles.getRadioNrf()->isInitialized() && (!Hoymiles.getRadioNrf()->isConnected() || !Hoymiles.getRadioNrf()->isPVariant())) || (Hoymiles.getRadioCmt()->isInitialized() && (!Hoymiles.getRadioCmt()->isConnected()));
    #endif
    hintObj["default_password"] = strcmp(Configuration.get().Security.Password, ACCESS_POINT_PASSWORD) == 0;
}

void WebApiWsLiveClass::generateInverterCommonJsonResponse(JsonObject& root, std::shared_ptr<BaseInverterClass> inv)
{
    const INVERTER_CONFIG_T* inv_cfg = Configuration.getInverterConfig(inv->serial());
    if (inv_cfg == nullptr) {
        return;
    }

    root["serial"] = inv->serialString();
    root["name"] = inv->name();
    root["order"] = inv_cfg->Order;
    root["data_age"] = (millis() - inv->getStatistics()->getLastUpdate()) / 1000;
    root["poll_enabled"] = inv->getEnablePolling();
    root["reachable"] = inv->isReachable();
    root["producing"] = inv->isProducing();
    root["manufacturer"] = from_inverter_type(inv->getInverterType());
    root["limit_relative"] = inv->getSystemConfigParaParser()->getLimitPercent();
    if (inv->getDevInfo()->getMaxPower() > 0) {
        root["limit_absolute"] = inv->getSystemConfigParaParser()->getLimitPercent() * inv->getDevInfo()->getMaxPower() / 100.0;
    } else {
        root["limit_absolute"] = -1;
    }
    #ifdef HOYMILES
    if(inv->getInverterType() == inverter_type::Inverter_Hoymiles) {
        auto hoy = reinterpret_cast<InverterAbstract *>(inv.get());
        root["radio_stats"]["tx_request"] = hoy->RadioStats.TxRequestData;
        root["radio_stats"]["tx_re_request"] = hoy->RadioStats.TxReRequestFragment;
        root["radio_stats"]["rx_success"] = hoy->RadioStats.RxSuccess;
        root["radio_stats"]["rx_fail_nothing"] = hoy->RadioStats.RxFailNoAnswer;
        root["radio_stats"]["rx_fail_partial"] = hoy->RadioStats.RxFailPartialAnswer;
        root["radio_stats"]["rx_fail_corrupt"] = hoy->RadioStats.RxFailCorruptData;
        root["radio_stats"]["rssi"] = hoy->getLastRssi();
    }
    #endif
}

void WebApiWsLiveClass::generateInverterChannelJsonResponse(JsonObject& root, std::shared_ptr<BaseInverterClass> inv)
{
    const INVERTER_CONFIG_T* inv_cfg = Configuration.getInverterConfig(inv->serial());
    if (inv_cfg == nullptr) {
        return;
    }

    // Loop all channels
    for (auto& t : inv->getStatistics()->getChannelTypes()) {
        auto chanTypeObj = root[inv->getStatistics()->getChannelTypeName(t)].to<JsonObject>();
        for (auto& c : inv->getStatistics()->getChannelsByType(t)) {
            if (t == TYPE_DC) {
                chanTypeObj[String(static_cast<uint8_t>(c))]["name"]["u"] = inv_cfg->channel[c].Name;
            }
            addField(chanTypeObj, inv, t, c, FLD_PAC);
            addField(chanTypeObj, inv, t, c, FLD_UAC);
            addField(chanTypeObj, inv, t, c, FLD_IAC);
            if (t == TYPE_INV) {
                addField(chanTypeObj, inv, t, c, FLD_PDC, "Power DC");
            } else {
                addField(chanTypeObj, inv, t, c, FLD_PDC);
            }
            addField(chanTypeObj, inv, t, c, FLD_UDC);
            addField(chanTypeObj, inv, t, c, FLD_IDC);
            addField(chanTypeObj, inv, t, c, FLD_YD);
            addField(chanTypeObj, inv, t, c, FLD_YT);
            addField(chanTypeObj, inv, t, c, FLD_F);
            addField(chanTypeObj, inv, t, c, FLD_T);
            addField(chanTypeObj, inv, t, c, FLD_PF);
            addField(chanTypeObj, inv, t, c, FLD_Q);
            addField(chanTypeObj, inv, t, c, FLD_EFF);
            if (t == TYPE_DC && inv->getStatistics()->getStringMaxPower(c) > 0) {
                addField(chanTypeObj, inv, t, c, FLD_IRR);
                chanTypeObj[String(c)][inv->getStatistics()->getChannelFieldName(t, c, FLD_IRR)]["max"] = inv->getStatistics()->getStringMaxPower(c);
            }
        }
    }

    if (inv->getStatistics()->hasChannelFieldValue(TYPE_INV, CH0, FLD_EVT_LOG)) {
        root["events"] = inv->getEventLog()->getEntryCount();
    } else {
        root["events"] = -1;
    }
}

void WebApiWsLiveClass::addField(JsonObject& root, std::shared_ptr<BaseInverterClass> inv, const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId, String topic)
{
    if (inv->getStatistics()->hasChannelFieldValue(type, channel, fieldId)) {
        String chanName;
        if (topic == "") {
            chanName = inv->getStatistics()->getChannelFieldName(type, channel, fieldId);
        } else {
            chanName = topic;
        }
        String chanNum;
        chanNum = channel;
        root[chanNum][chanName]["v"] = inv->getStatistics()->getChannelFieldValue(type, channel, fieldId);
        root[chanNum][chanName]["u"] = inv->getStatistics()->getChannelFieldUnit(type, channel, fieldId);
        root[chanNum][chanName]["d"] = inv->getStatistics()->getChannelFieldDigits(type, channel, fieldId);
    }
}

void WebApiWsLiveClass::addTotalField(JsonObject& root, const String& name, const float value, const String& unit, const uint8_t digits)
{
    root[name]["v"] = value;
    root[name]["u"] = unit;
    root[name]["d"] = digits;
}

void WebApiWsLiveClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        MessageOutput.printf("Websocket: [%s][%u] connect\r\n", server->url(), client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        MessageOutput.printf("Websocket: [%s][%u] disconnect\r\n", server->url(), client->id());
    }
}

void WebApiWsLiveClass::onLivedataStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(_mutex);
        AsyncJsonResponse* response = new AsyncJsonResponse();
        auto& root = response->getRoot();
        auto invArray = root["inverters"].to<JsonArray>();
        auto serial = WebApi.parseSerialFromRequest(request);
        auto type = root["manufacturer"].as<String>();

        if (serial > 0) {
            auto inv = InverterHandler.getInverterBySerial(serial, to_inverter_type(type));
            if (inv != nullptr) {
                JsonObject invObject = invArray.add<JsonObject>();
                generateInverterCommonJsonResponse(invObject, inv);
                generateInverterChannelJsonResponse(invObject, inv);
            }
        } else {
            // Loop all inverters
            for (uint8_t i = 0; i < InverterHandler.getNumInverters(); i++) {
                auto inv = InverterHandler.getInverterByPos(i);
                if (inv == nullptr) {
                    continue;
                }

                JsonObject invObject = invArray.add<JsonObject>();
                generateInverterCommonJsonResponse(invObject, inv);
            }
        }

        generateCommonJsonResponse(root);

        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    } catch (const std::bad_alloc& bad_alloc) {
        MessageOutput.printf("Call to /api/livedata/status temporarely out of resources. Reason: \"%s\".\r\n", bad_alloc.what());
        WebApi.sendTooManyRequests(request);
    } catch (const std::exception& exc) {
        MessageOutput.printf("Unknown exception in /api/livedata/status. Reason: \"%s\".\r\n", exc.what());
        WebApi.sendTooManyRequests(request);
    }
}
