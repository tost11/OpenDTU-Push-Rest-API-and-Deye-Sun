// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TimeoutHelper.h>
#include <iostream>
#include <Hoymiles.h>
#include "Configuration.h"
#include <TaskSchedulerDeclarations.h>
#include <ArduinoJson.h>
#include <future>
#include <HTTPClient.h>

struct http_requests_to_send{
    http_requests_to_send(uint32_t timestamp):
            timestamp(timestamp){}
    String data;
    uint32_t timestamp;
};

class TostHandleClass {
public:
    void init(Scheduler& scheduler);

private:
    Task _loopTask;
    void loop();

    std::optional<std::pair<int,std::unique_ptr<HTTPClient>>> _lastRequestResponse;
    std::unique_ptr<http_requests_to_send> _currentlySendingData;

    //TimeoutHelper _lastPublish;
    TimeoutHelper _cleanupCheck;

    std::unordered_map<std::string,uint32_t> _lastPublishedInverters;

    std::queue<std::unique_ptr<http_requests_to_send>> requestsToSend;

    int lastErrorStatusCode;
    std::string lastErrorMessage;

    long lastErrorTimestamp;
    long lastSuccessfullyTimestamp;

    const long TIMER_CLEANUP = 1000 * 60 * 5;

    FieldId_t _publishFields[14] = {
            FLD_UDC,
            FLD_IDC,
            FLD_PDC,
            FLD_YD,
            FLD_YT,
            FLD_UAC,
            FLD_IAC,
            FLD_PAC,
            FLD_F,
            FLD_T,
            FLD_PF,
            FLD_EFF,
            FLD_IRR,
            FLD_Q
    };

    void handleResponse();

    void runNextHttpRequest();

    std::thread _runningThread;

public:
    unsigned long getLastErrorTimestamp()const{return lastErrorTimestamp;}
    unsigned long getLastSuccessfullyTimestamp()const{return lastSuccessfullyTimestamp;}
    int getLastErrorStatusCode()const{return lastErrorStatusCode;}
    const std::string & getLastErrorMessage()const{return lastErrorMessage;}

};

extern TostHandleClass TostHandle;