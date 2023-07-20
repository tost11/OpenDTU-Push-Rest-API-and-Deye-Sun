// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "WebApi_tost.h"
#include "Configuration.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "helper.h"
#include <AsyncJson.h>
#include "TostHandle.h"

void WebApiTostClass::init(AsyncWebServer* server)
{
    using std::placeholders::_1;

    _server = server;

    _server->on("/api/tost/status", HTTP_GET, std::bind(&WebApiTostClass::onTostStatus, this, _1));
    _server->on("/api/tost/config", HTTP_GET, std::bind(&WebApiTostClass::onTostAdminGet, this, _1));
    _server->on("/api/tost/config", HTTP_POST, std::bind(&WebApiTostClass::onTostAdminPost, this, _1));
}

void WebApiTostClass::loop()
{
}

void WebApiTostClass::onTostStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse(false, TOST_JSON_DOC_SIZE);
    JsonObject root = response->getRoot();
    const CONFIG_T& config = Configuration.get();

    root[F("tost_enabled")] = config.Tost_Enabled;
    root[F("tost_url")] = config.Tost_Url;
    root[F("tost_system_id")] = config.Tost_System_Id;
    root[F("tost_duration")] = config.Tost_Duration;
    root[F("tost_status_successfully_timestamp")] = TostHandle.getLastSuccessfullyTimestamp();
    root[F("tost_status_error_static_code")] = TostHandle.getLastErrorStatusCode();

    response->setLength();
    request->send(response);
}

void WebApiTostClass::onTostAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse(false, TOST_JSON_DOC_SIZE);
    JsonObject root = response->getRoot();
    const CONFIG_T& config = Configuration.get();

    root[F("tost_enabled")] = config.Tost_Enabled;
    root[F("tost_url")] = config.Tost_Url;
    root[F("tost_system_id")] = config.Tost_System_Id;
    root[F("tost_token")] = config.Tost_Token;
    root[F("tost_duration")] = config.Tost_Duration;

    response->setLength();
    request->send(response);
}

void WebApiTostClass::onTostAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse(false, TOST_JSON_DOC_SIZE);
    JsonObject retMsg = response->getRoot();
    retMsg[F("type")] = F("warning");

    if (!request->hasParam("data", true)) {
        retMsg[F("message")] = F("No values found!");
        retMsg[F("code")] = WebApiError::GenericNoValueFound;
        response->setLength();
        request->send(response);
        return;
    }

    String json = request->getParam("data", true)->value();

    if (json.length() > TOST_JSON_DOC_SIZE) {
        retMsg[F("message")] = F("Data too large!");
        retMsg[F("code")] = WebApiError::GenericDataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(TOST_JSON_DOC_SIZE);
    DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg[F("message")] = F("Failed to parse data!");
        retMsg[F("code")] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("tost_enabled")
            && root.containsKey("tost_url")
            && root.containsKey("tost_system_id")
            && root.containsKey("tost_token")
            && root.containsKey("tost_duration"))) {
        retMsg[F("message")] = F("Values are missing!");
        retMsg[F("code")] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    if (root[F("tost_enabled")].as<bool>()) {
        if (root[F("tost_url")].as<String>().length() == 0 || root[F("tost_url")].as<String>().length() > TOST_MAX_URL_STRLEN) {
            retMsg[F("message")] = F("Monitoring Url must between 1 and " STR(TOST_MAX_URL_STRLEN) " characters long!");
            retMsg[F("code")] = WebApiError::TostUrlLength;
            retMsg[F("param")][F("max")] = TOST_MAX_URL_STRLEN;
            response->setLength();
            request->send(response);
            return;
        }

        if (root[F("tost_system_id")].as<String>().length() == 0 || root[F("tost_system_id")].as<String>().length() > TOST_MAX_SYSTEM_ID_STRLEN) {
            retMsg[F("message")] = F("Monitoring System Id must between 1 and " STR(TOST_MAX_SYSTEM_ID_STRLEN) " characters long!");
            retMsg[F("code")] = WebApiError::TostSystemIdLength;
            retMsg[F("param")][F("max")] = TOST_MAX_SYSTEM_ID_STRLEN;
            response->setLength();
            request->send(response);
            return;
        }
        if (root[F("tost_token")].as<String>().length() == 0 || root[F("tost_token")].as<String>().length() > TOST_MAX_TOKEN_STRLEN) {
            retMsg[F("message")] = F("Password must not longer then " STR(TOST_MAX_TOKEN_STRLEN) " characters!");
            retMsg[F("code")] = WebApiError::TostTokenLength;
            retMsg[F("param")][F("max")] = TOST_MAX_TOKEN_STRLEN;
            response->setLength();
            request->send(response);
            return;
        }

        if (root[F("tost_duration")].as<uint>() == 0 || root[F("tost_duration")].as<uint>() > 1000) {
            retMsg[F("message")] = F("Port must be a number between 0 and 1000!");
            retMsg[F("code")] = WebApiError::TostDuration;
            response->setLength();
            request->send(response);
            return;
        }
    }

    CONFIG_T& config = Configuration.get();
    config.Tost_Enabled = root[F("tost_enabled")].as<bool>();
    config.Tost_Duration = root[F("tost_duration")].as<uint>();
    strlcpy(config.Tost_Url, root[F("tost_url")].as<String>().c_str(), sizeof(config.Tost_Url));
    strlcpy(config.Tost_System_Id, root[F("tost_system_id")].as<String>().c_str(), sizeof(config.Tost_System_Id));
    strlcpy(config.Tost_Token, root[F("tost_token")].as<String>().c_str(), sizeof(config.Tost_Token));
    Configuration.write();

    retMsg[F("type")] = F("success");
    retMsg[F("message")] = F("Settings saved!");
    retMsg[F("code")] = WebApiError::GenericSuccess;

    response->setLength();
    request->send(response);

    //MqttSettings.performReconnect();
    //MqttHandleHass.forceUpdate();
}
