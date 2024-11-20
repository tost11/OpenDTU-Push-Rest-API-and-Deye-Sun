#pragma once

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

#define SERVO_JSON_DOC_SIZE 1024

class WebApiServoClass {
public:
    void init(AsyncWebServer& server,Scheduler& scheduler);
    void loop();

private:
    void onServoAdminGet(AsyncWebServerRequest* request);
    void onServoAdminPost(AsyncWebServerRequest* request);

    AsyncWebServer* _server;
};