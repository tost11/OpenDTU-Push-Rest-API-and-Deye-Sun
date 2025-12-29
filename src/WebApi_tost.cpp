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
#include "MessageOutput.h"

void WebApiTostClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/tost/status", HTTP_GET, std::bind(&WebApiTostClass::onTostStatus, this, _1));
    server.on("/api/tost/config", HTTP_GET, std::bind(&WebApiTostClass::onTostAdminGet, this, _1));
    server.on("/api/tost/config", HTTP_POST, std::bind(&WebApiTostClass::onTostAdminPost, this, _1));
}

void WebApiTostClass::loop()
{
}

void WebApiTostClass::onTostStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    const CONFIG_T& config = Configuration.get();

    unsigned long errorStamp = TostHandle.getLastErrorTimestamp();
    unsigned long successStamp = TostHandle.getLastSuccessfullyTimestamp();

    root["tost_enabled"] = config.Tost.Enabled;
    root["tost_url"] = config.Tost.Url;
    root["tost_second_url"] = config.Tost.SecondUrl;
    root["tost_system_id"] = config.Tost.SystemId;
    root["tost_duration"] = config.Tost.Duration;
    root["tost_status_successfully_timestamp"] = successStamp == 0 ? successStamp : millis() - successStamp;
    root["tost_status_error_code"] = TostHandle.getLastErrorStatusCode();
    root["tost_status_error_message"] = TostHandle.getLastErrorMessage();
    root["tost_status_error_timestamp"] = errorStamp == 0 ? errorStamp : millis() - errorStamp;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiTostClass::onTostAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    const CONFIG_T& config = Configuration.get();

    root["tost_enabled"] = config.Tost.Enabled;
    root["tost_url"] = config.Tost.Url;
    root["tost_second_url"] = config.Tost.SecondUrl;
    root["tost_system_id"] = config.Tost.SystemId;
    root["tost_token"] = config.Tost.Token;
    root["tost_duration"] = config.Tost.Duration;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiTostClass::onTostAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    retMsg["type"] = "warning";

    if (!request->hasParam("data", true)) {
        retMsg["message"] = "No values found!";
        retMsg["code"] = WebApiError::GenericNoValueFound;
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root["tost_enabled"].is<bool>()
            && root["tost_url"].is<String>()
            && root["tost_second_url"].is<String>()
            && root["tost_system_id"].is<String>()
            && root["tost_token"].is<String>()
            && root["tost_duration"].is<uint>())) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root["tost_enabled"].as<bool>()) {
        if (root["tost_url"].as<String>().length() == 0 || root["tost_url"].as<String>().length() > TOST_MAX_URL_STRLEN) {
            retMsg["message"] = "Monitoring Url must between 1 and " STR(TOST_MAX_URL_STRLEN) " characters long!";
            retMsg["code"] = WebApiError::TostUrlLength;
            retMsg["param"]["max"] = TOST_MAX_URL_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["tost_second_url"].as<String>().length() > TOST_MAX_URL_STRLEN) {
            retMsg["message"] = "Second Monitoring Url must between 1 and " STR(TOST_MAX_URL_STRLEN) " characters long!";
            retMsg["code"] = WebApiError::TostSecondUrlLength;
            retMsg["param"]["max"] = TOST_MAX_URL_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if (root["tost_system_id"].as<String>().length() == 0 || root["tost_system_id"].as<String>().length() > TOST_MAX_SYSTEM_ID_STRLEN) {
            retMsg["message"] = "Monitoring System Id must between 1 and " STR(TOST_MAX_SYSTEM_ID_STRLEN) " characters long!";
            retMsg["code"] = WebApiError::TostSystemIdLength;
            retMsg["param"]["max"] = TOST_MAX_SYSTEM_ID_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }
        if (root["tost_token"].as<String>().length() == 0 || root["tost_token"].as<String>().length() > TOST_MAX_TOKEN_STRLEN) {
            retMsg["message"] = "Password must not longer then " STR(TOST_MAX_TOKEN_STRLEN) " characters!";
            retMsg["code"] = WebApiError::TostTokenLength;
            retMsg["param"]["max"] = TOST_MAX_TOKEN_STRLEN;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        if ( root["tost_duration"].as<uint>() > 60 * 100) {//10 min
            retMsg["message"] = "Port must be a number between 0 and 600!";
            retMsg["code"] = WebApiError::TostDuration;
            retMsg["param"]["min"] = 0;
            retMsg["param"]["max"] = 60 * 100;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }
    }

    {
        auto guard = Configuration.getWriteGuard();
        auto& config = guard.getConfig();
        config.Tost.Enabled = root["tost_enabled"].as<bool>();
        config.Tost.Duration = root["tost_duration"].as<uint>();
        strlcpy(config.Tost.Url, root["tost_url"].as<String>().c_str(), sizeof(config.Tost.Url));
        strlcpy(config.Tost.SecondUrl, root["tost_second_url"].as<String>().c_str(), sizeof(config.Tost.SecondUrl));
        strlcpy(config.Tost.SystemId, root["tost_system_id"].as<String>().c_str(), sizeof(config.Tost.SystemId));
        strlcpy(config.Tost.Token, root["tost_token"].as<String>().c_str(), sizeof(config.Tost.Token));
        Configuration.write();
    }

    retMsg["type"] = "success";
    retMsg["message"] = "Settings saved!";
    retMsg["code"] = WebApiError::GenericSuccess;
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}
