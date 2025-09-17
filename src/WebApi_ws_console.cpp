// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2025 Thomas Basler and others
 */
#include "WebApi_ws_console.h"
#include "Configuration.h"
#include <MessageOutput.h>
#include "WebApi.h"
#include "defaults.h"

#include <MessageOutput.h>

WebApiWsConsoleClass::WebApiWsConsoleClass()
    : _ws("/console")
    , _wsCleanupTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsConsoleClass::wsCleanupTaskCb, this))
{
}

void WebApiWsConsoleClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/console/debug", HTTP_GET, std::bind(&WebApiWsConsoleClass::onDebugGet, this, _1));
    server.on("/api/console/debug", HTTP_POST, std::bind(&WebApiWsConsoleClass::onDebugSet, this, _1));

    server.addHandler(&_ws);
    MessageOutput.register_ws_output(&_ws);

    scheduler.addTask(_wsCleanupTask);
    _wsCleanupTask.enable();

    _simpleDigestAuth.setUsername(AUTH_USERNAME);
    _simpleDigestAuth.setRealm("console websocket");

    reload();
}

void WebApiWsConsoleClass::reload()
{
    _ws.removeMiddleware(&_simpleDigestAuth);

    auto const& config = Configuration.get();

    if (config.Security.AllowReadonly) {
        return;
    }

    _ws.enable(false);
    _simpleDigestAuth.setPassword(config.Security.Password);
    _ws.addMiddleware(&_simpleDigestAuth);
    _ws.closeAll();
    _ws.enable(true);
}

void WebApiWsConsoleClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}

void WebApiWsConsoleClass::onDebugGet(AsyncWebServerRequest* request){
    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();

    root["debugLogging"] = MessageOutput.logDebug;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}


void WebApiWsConsoleClass::onDebugSet(AsyncWebServerRequest* request){
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    if (!(root["debugLogging"].is<bool>())) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    MessageOutput.logDebug = root["debugLogging"].as<bool>();

    auto& ret = response->getRoot();

    ret["debugLogging"] = MessageOutput.logDebug;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}