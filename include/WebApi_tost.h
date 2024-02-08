// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

#define TOST_JSON_DOC_SIZE 3072

class WebApiTostClass {
public:
    void init(AsyncWebServer& server,Scheduler& scheduler);
    void loop();

private:
    void onTostStatus(AsyncWebServerRequest* request);
    void onTostAdminGet(AsyncWebServerRequest* request);
    void onTostAdminPost(AsyncWebServerRequest* request);

    AsyncWebServer* _server;
};