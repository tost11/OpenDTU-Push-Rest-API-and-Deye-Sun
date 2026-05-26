// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TimeoutHelper.h>
#include "Configuration.h"
#include "inverters/InverterAbstract.h"
#include <TaskSchedulerDeclarations.h>
#include <ArduinoJson.h>
#include <queue>
#include "RestRequestHandler.h"

class TostHandleClass {
public:
    void init(Scheduler& scheduler);

private:
    Task _loopTask;
    void loop();

    uint8_t getEffectiveMaxQueueSize() const;
    void trimQueueToSize();

    struct ActiveRequest {
        LightFuture<RestResponse> future;
        bool isSecondaryUrl;
    };
    std::optional<ActiveRequest> _activeRequest;  // Only 0 or 1 active request
    std::queue<std::unique_ptr<String>> requestsToSend;  // Local buffer of unsent data
    size_t _queueMemoryBytes = 0;
    String _lastRequestBody;

    //TimeoutHelper _lastPublish;
    TimeoutHelper _cleanupCheck;

    std::unordered_map<std::string,uint32_t> _lastPublishedInverters;

    int lastErrorStatusCode = 0;
    String lastErrorMessage;

    unsigned long lastErrorTimestamp = 0;
    unsigned long lastSuccessfullyTimestamp = 0;
    TimeoutHelper restTimeout;

    const long TIMER_CLEANUP = 1000 * 60 * 5;

    void handleResponse(const RestResponse& response, bool isSecondaryUrl);
    void processActiveRequest();  // Check if active request is complete
    void sendNextRequest();        // Send next from queue to RestRequestHandler
    void queueSecondaryUrlRequest();

    static bool parseKWHValues(InverterAbstract *inv, JsonObject &doc, const ChannelType_t type, const ChannelNum_t channel) ;
public:
    unsigned long getLastErrorTimestamp()const{return lastErrorTimestamp;}
    unsigned long getLastSuccessfullyTimestamp()const{return lastSuccessfullyTimestamp;}
    int getLastErrorStatusCode()const{return lastErrorStatusCode;}
    const String & getLastErrorMessage()const{return lastErrorMessage;}
    size_t getQueueSize() const { return requestsToSend.size(); }
    uint8_t getMaxQueueSize() const { return Configuration.get().Tost.QueueSize; }
    size_t getQueueMemoryBytes() const;
};

extern TostHandleClass TostHandle;