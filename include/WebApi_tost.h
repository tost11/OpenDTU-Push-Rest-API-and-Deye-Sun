// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

class WebApiTostClass {
public:
    void init(AsyncWebServer& server,Scheduler& scheduler);
    void loop();

private:
    void onTostStatus(AsyncWebServerRequest* request);
    void onTostAdminGet(AsyncWebServerRequest* request);
    void onTostAdminPost(AsyncWebServerRequest* request);
};