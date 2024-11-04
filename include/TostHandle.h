// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TimeoutHelper.h>
#include "Configuration.h"
#include <TaskSchedulerDeclarations.h>
#include <ArduinoJson.h>
#include <queue>
#include <thread>

class HTTPClient;

class TostHandleClass {
public:
    void init(Scheduler& scheduler);

private:
    Task _loopTask;
    void loop();

    std::optional<std::pair<int,std::unique_ptr<HTTPClient>>> _lastRequestResponse;
    String * _currentlySendingData;

    //TimeoutHelper _lastPublish;
    TimeoutHelper _cleanupCheck;

    std::unordered_map<std::string,uint32_t> _lastPublishedInverters;

    std::queue<std::unique_ptr<String>> requestsToSend;

    int lastErrorStatusCode;
    String lastErrorMessage;

    long lastErrorTimestamp;
    long lastSuccessfullyTimestamp;
    TimeoutHelper restTimeout;

    const long TIMER_CLEANUP = 1000 * 60 * 5;

    void handleResponse();

    void runNextHttpRequest();

    std::thread _runningThread;

public:
    unsigned long getLastErrorTimestamp()const{return lastErrorTimestamp;}
    unsigned long getLastSuccessfullyTimestamp()const{return lastSuccessfullyTimestamp;}
    int getLastErrorStatusCode()const{return lastErrorStatusCode;}
    const String & getLastErrorMessage()const{return lastErrorMessage;}

};

extern TostHandleClass TostHandle;