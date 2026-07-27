// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TimeoutHelper.h>
#include "Configuration.h"
#include "inverters/InverterAbstract.h"
#include <TaskSchedulerDeclarations.h>
#include <ArduinoJson.h>
#include <queue>
#include <set>
#include "RestRequestHandler.h"
#include <mbedtls/sha256.h>
#include <mbedtls/gcm.h>
#include <esp_random.h>

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
    std::queue<String> requestsToSend;  // Local buffer of unsent data
    size_t _queueMemoryBytes = 0;

    //TimeoutHelper _lastPublish;
    TimeoutHelper _cleanupCheck;
    TimeoutHelper _statsLog;

    std::unordered_map<std::string,uint32_t> _lastPublishedInverters;

    int lastErrorStatusCode = 0;
    String lastErrorMessage;

    unsigned long lastErrorTimestamp = 0;
    unsigned long lastSuccessfullyTimestamp = 0;
    TimeoutHelper restTimeout;

    const long TIMER_CLEANUP = 1000 * 60 * 5;

    JsonDocument _jsonDoc;  // Reusable JSON document - internal pool stabilizes after first few uses

    std::string generateUniqueId(const BaseInverterClass & inv);

    void handleResponse(const RestResponse& response, bool isSecondaryUrl);
    void processActiveRequest();  // Check if active request is complete
    void sendNextRequest();        // Send next from queue to RestRequestHandler
    void queueSecondaryUrlRequest();
    void sendToRedirectUrl(const String& locationUrl, bool isSecondaryUrl);

    // Redirect tracking
    static const uint8_t MAX_REDIRECTS = 5;
    std::set<String> _visitedRedirectUrls;

    // AES-256-GCM encryption
    uint8_t _encryptionKey[32];
    bool _encryptionKeyValid = false;
    bool encryptBody(const String& plaintext, const char* systemId,
                     String& ciphertext, char nonceHex[25]) const;
    static String buildUrl(const char* host, const char* path);

    static bool parseKWHValues(BaseInverterClass *inv, JsonObject &doc, const ChannelType_t type, const ChannelNum_t channel) ;
public:
    unsigned long getLastErrorTimestamp()const{return lastErrorTimestamp;}
    unsigned long getLastSuccessfullyTimestamp()const{return lastSuccessfullyTimestamp;}
    int getLastErrorStatusCode()const{return lastErrorStatusCode;}
    const String & getLastErrorMessage()const{return lastErrorMessage;}
    size_t getQueueSize() const { return requestsToSend.size(); }
    uint8_t getMaxQueueSize() const { return Configuration.get().Tost.QueueSize; }
    size_t getQueueMemoryBytes() const;
    void deriveEncryptionKey();
};

extern TostHandleClass TostHandle;