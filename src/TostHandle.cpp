// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Thomas Basler and others
 */
#include "TostHandle.h"

#if TOST

#include "Configuration.h"
#include "Datastore.h"
#include <MessageOutput.h>
#include "InverterHandler.h"
#include "NtpSettings.h"
#include <ctime>
#include <ArduinoJson.h>
#include "RestRequestHandler.h"
#include <chrono>

TostHandleClass TostHandle;

#undef TAG
static const char* TAG = "rest-push";

void TostHandleClass::init(Scheduler& scheduler)
{
    //_lastPublish.set(Configuration.get().Tost.Duration * 1000);
    _cleanupCheck.set(TIMER_CLEANUP);
    _statsLog.set(60 * 1000);
    lastErrorStatusCode = 0;
    lastErrorTimestamp = 0;
    lastSuccessfullyTimestamp = 0;
    restTimeout.set(0);
    lastErrorMessage = "";
    _queueMemoryBytes = 0;

    deriveEncryptionKey();

    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&TostHandleClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.setInterval(1 * TASK_SECOND);
    _loopTask.enable();
}

uint8_t TostHandleClass::getEffectiveMaxQueueSize() const {
    uint8_t maxSize = Configuration.get().Tost.QueueSize;
    if(maxSize == 0) {
        return 1;
    }
    return maxSize;
}

void TostHandleClass::trimQueueToSize() {
    uint8_t maxSize = getEffectiveMaxQueueSize();
    while(requestsToSend.size() > maxSize) {
        ESP_LOGW(TAG, "Queue exceeds max size (%d), dropping oldest", maxSize);
        _queueMemoryBytes -= requestsToSend.front().length();
        requestsToSend.pop();
    }
}

size_t TostHandleClass::getQueueMemoryBytes() const {
    return _queueMemoryBytes;
}

bool TostHandleClass::parseKWHValues(BaseInverterClass * inv, JsonObject & doc, const ChannelType_t type, const ChannelNum_t channel) {
    bool changed = false;

    // Total yield (lifetime): only send if > 0
    if(inv->getStatistics()->hasChannelFieldValue(type, channel, FLD_YT)) {
        float totalKWH = inv->getStatistics()->getChannelFieldValue(type, channel, FLD_YT) / (inv->getStatistics()->getChannelFieldUnitId(type,channel,FLD_YT) == UNIT_WH ? 1000.f : 1.f);
        if (totalKWH > 0) {
            doc["totalKWH"] = totalKWH;
            changed = true;
        }
    }

    // Daily yield: only send if >= 0
    if(inv->getStatistics()->hasChannelFieldValue(type, channel, FLD_YD)) {
        float dailyKWH = inv->getStatistics()->getChannelFieldValue(type, channel, FLD_YD) / (inv->getStatistics()->getChannelFieldUnitId(type,channel,FLD_YD) == UNIT_WH ? 1000.f : 1.f);
        if (dailyKWH >= 0) {
            doc["dailyKWH"] = dailyKWH;
            changed = true;
        }
    }

    return changed;
}

void TostHandleClass::loop()
{
    //channel 0 -> inverter
    //5: voltage
    //6: ampere
    //8: frequenz
    //2: watt solar
    //7: watt output
    //3: tagesertrag wh
    //4: gesamtertrag kwh

    //chanel 1 -> dc input
    //0: voltage
    //2: watt
    //4: geamtertrag
    //1: ampere

    //channel 2 -> temperature
    //9: temperature

    // Log REST stats every 60s regardless of state
    if (_statsLog.occured()) {
        _statsLog.set(60 * 1000);
        RestRequestHandler.printStats();
    }

    // Always process active requests regardless of radio state or enabled flag.
    // Prevents permanent stall when radio goes busy while a response is in flight.
    processActiveRequest();

    if (!Configuration.get().Tost.Enabled || !InverterHandler.isAllRadioIdle()) {
        return;
    }

    // 1. Trim queue if config changed
    trimQueueToSize();

    // 3. If no active request, try to send next from queue
    if (!_activeRequest.has_value() && !requestsToSend.empty() && restTimeout.occured()) {
        sendNextRequest();
    }

    // 4. Run cleanup check
    if(_cleanupCheck.occured()){
        ESP_LOGD(TAG,"Run cleanup");
    _cleanupCheck.set(TIMER_CLEANUP);
    _statsLog.set(60 * 1000);

        for (auto it = _lastPublishedInverters.begin(); it != _lastPublishedInverters.end(); ) {
            bool found = false;
            for (uint8_t i = 0; i < InverterHandler.getNumInverters(); i++) {
                auto inv = InverterHandler.getInverterByPos(i);
                if (inv->getDevInfo()->getLastUpdate() <= 0) {
                    continue;
                }
                if (generateUniqueId(*inv) == it->first) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                ESP_LOGD(TAG,"cleaned: %s", it->first.c_str());
                it = _lastPublishedInverters.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 4. Collect inverter data and add to queue (existing logic)
    for (uint8_t i = 0; i < InverterHandler.getNumInverters(); i++) {

        auto inv = InverterHandler.getInverterByPos(i);
        if (inv->getStatistics()->getLastUpdate() <= 0) {
            continue;
        }

        std::string uniqueID = generateUniqueId(*inv);
        uint32_t cachedLastUpdate = 0;
        auto it = _lastPublishedInverters.find(uniqueID);
        if(it != _lastPublishedInverters.end()){
            cachedLastUpdate = it->second;
        }

        uint32_t lastUpdate = inv->getStatistics()->getLastUpdate();

        if(lastUpdate <= 0 || lastUpdate == cachedLastUpdate){
            continue;
        }

        uint32_t diff;
        if(cachedLastUpdate > lastUpdate){
            //overrun of millseconds timer
            diff = lastUpdate + (std::numeric_limits<uint32_t>::max() - cachedLastUpdate);
        }else{
            diff = lastUpdate - cachedLastUpdate;
        }

        //ESP_LOGD(TAG,"last: %d ",lastUpdate);
        //ESP_LOGD(TAG,"calc: %d ",cachedLastUpdate);
        //ESP_LOGD(TAG,"diff: %d\n",diff);

        if(cachedLastUpdate != 0 && diff < Configuration.get().Tost.Duration * 1000){
            //no update needed
            continue;
        }

        uint64_t id = inv->serial();

        ESP_LOGI(TAG,"New data to push for Inverter %llu\n\r", id);
        _lastPublishedInverters[uniqueID] = lastUpdate;

        _jsonDoc.clear();

        float duration = (float)diff / 1000;

        if(duration > Configuration.get().Tost.Duration * 1.2){
            duration = Configuration.get().Tost.Duration * 1.2;
        }

        _jsonDoc["duration"] = duration;
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            time_t now;
            time(&now);
            _jsonDoc["timeUnit"] = "SECONDS";
            if(NtpSettings.isTimeInSync()){
                _jsonDoc["timestamp"] = time(&now);
            }else{
                _jsonDoc["timestamp"] = 0;
            }
            ESP_LOGD(TAG,"Time set on new inverter info manually %lu", time(&now));
        }

        JsonArray devices = _jsonDoc["devices"].to<JsonArray>();
        auto device = devices.add<JsonObject>();
        device["id"] = id;

        JsonArray inputs = device["inputsDC"].to<JsonArray>();
        JsonArray outputs = device["outputsAC"].to<JsonArray>();

        int inputCount = 0;
        int outputCount = 0;

        bool isData = false;

        // Loop all channels
        for (auto& channelType : inv->getStatistics()->getChannelTypes()) {
            for (auto& c : inv->getStatistics()->getChannelsByType(channelType)) {

                //MessageOutput.printf("Next Channel: %d\n\r",channelType);

                if(channelType == 0){//inverter
                    isData = true;
                    auto output = outputs.add<JsonObject>();
                    output["id"] = outputCount++;
                    output["voltage"] = inv->getStatistics()->getChannelFieldValue(channelType, c, FLD_UAC);
                    output["ampere"] = inv->getStatistics()->getChannelFieldValue(channelType, c, FLD_IAC);
                    output["watt"] = inv->getStatistics()->getChannelFieldValue(channelType, c, FLD_PAC);
                    output["frequency"] = inv->getStatistics()->getChannelFieldValue(channelType, c, FLD_F);
                    parseKWHValues(inv.get(),output,channelType,c);
                }else if(channelType == 1){
                    isData = true;
                    auto input = inputs.add<JsonObject>();
                    input["id"] = inputCount++;
                    input["voltage"] = inv->getStatistics()->getChannelFieldValue(channelType, c, FLD_UDC);
                    input["ampere"] = inv->getStatistics()->getChannelFieldValue(channelType, c, FLD_IDC);
                    input["watt"] = inv->getStatistics()->getChannelFieldValue(channelType, c, FLD_PDC);
                    parseKWHValues(inv.get(),input,channelType,c);
                }else if(channelType == 2){
                    if(inv->getStatistics()->hasChannelFieldValue(channelType, c, FLD_T)) {
                        isData = true;
                        device["temperature"] = inv->getStatistics()->getChannelFieldValue(channelType, c, FLD_T);
                    }
                    if(parseKWHValues(inv.get(),device,channelType,c)){
                        isData = true;
                    }
                }

                /*for (uint8_t f = 0; f < sizeof(_publishFields) / sizeof(FieldId_t); f++) {
                    MessageOutput.printf("%d: %f\n\r",_publishFields[f],inv->Statistics()->getChannelFieldValue(channelType, c, _publishFields[f]));
                }*/
            }
        }

        if(isData){
            // Serialize and add to local queue
            String toSend;
            serializeJson(_jsonDoc, toSend);
            size_t stringSize = toSend.length();

            // If queue full, remove oldest
            uint8_t maxSize = getEffectiveMaxQueueSize();
            if(requestsToSend.size() >= maxSize) {
                ESP_LOGW(TAG, "Request queue full (%d), dropping oldest", maxSize);
                _queueMemoryBytes -= requestsToSend.front().length();
                requestsToSend.pop();
            }

            ESP_LOGD(TAG, "Adding new request to queue (size: %d)", requestsToSend.size() + 1);
            _queueMemoryBytes += stringSize;
            requestsToSend.push(std::move(toSend));
        }
    }
}

std::string TostHandleClass::generateUniqueId(const BaseInverterClass &inv) {
    return (from_inverter_type(inv.getInverterType()) + inv.serialString()).c_str();
}

void TostHandleClass::deriveEncryptionKey() {
    const char* token = Configuration.get().Tost.Token;
    if (strlen(token) == 0) {
        _encryptionKeyValid = false;
        ESP_LOGW(TAG, "No clientToken configured — encryption disabled");
        return;
    }
    mbedtls_sha256(
        (const unsigned char*)token, strlen(token),
        _encryptionKey, 0  // 0 = SHA-256 (not SHA-224)
    );
    _encryptionKeyValid = true;
    ESP_LOGI(TAG, "Encryption key derived from clientToken");
}

bool TostHandleClass::encryptBody(const String& plaintext,
                                   const char* systemId,
                                   String& ciphertext,
                                   char nonceHex[25]) const {
    if (!_encryptionKeyValid) {
        ESP_LOGE(TAG, "Encryption key not valid");
        return false;
    }

    // 12-byte random nonce via ESP32 hardware RNG (entropy-seeded by WiFi)
    uint8_t nonce[12];
    esp_fill_random(nonce, sizeof(nonce));

    // Encode nonce as 24 hex chars for X-Nonce header
    for (int i = 0; i < 12; i++) {
        snprintf(nonceHex + i * 2, 3, "%02x", nonce[i]);
    }
    nonceHex[24] = '\0';

    // Output buffer: plaintext + 16-byte GCM auth tag
    size_t plainLen = plaintext.length();
    size_t cipherLen = plainLen + 16;  // 16-byte auth tag appended

    uint8_t* buf = (uint8_t*)malloc(cipherLen);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate ciphertext buffer (%u bytes)", (unsigned)cipherLen);
        return false;
    }

    // AES-256-GCM encryption with systemId as AAD
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, _encryptionKey, 256);
    if (rc != 0) {
        ESP_LOGE(TAG, "GCM setkey failed (rc=%d)", rc);
        mbedtls_gcm_free(&gcm);
        free(buf);
        return false;
    }

    // Encrypt: output = ciphertext (plainLen bytes) followed by tag (16 bytes)
    rc = mbedtls_gcm_crypt_and_tag(
        &gcm,
        MBEDTLS_GCM_ENCRYPT,
        plainLen,
        nonce, sizeof(nonce),                          // 12-byte IV
        (const uint8_t*)systemId, strlen(systemId),    // AAD = systemId
        (const uint8_t*)plaintext.c_str(),             // input plaintext
        buf,                                            // output ciphertext (same length as input)
        16,                                             // tag length
        buf + plainLen                                  // output tag (appended after ciphertext)
    );

    mbedtls_gcm_free(&gcm);

    if (rc != 0) {
        ESP_LOGE(TAG, "AES-GCM encryption failed (rc=%d)", rc);
        free(buf);
        return false;
    }

    ciphertext = String((char*)buf, (unsigned int)cipherLen);
    free(buf);
    return true;
}

String TostHandleClass::buildUrl(const char* host, const char* path) {
    String url = "http://";
    String h = host;
    // Strip any existing scheme prefix (backwards compatibility with old config)
    if (h.startsWith("https://")) h = h.substring(8);
    else if (h.startsWith("http://")) h = h.substring(7);
    url += h;
    url += path;
    return url;
}

void TostHandleClass::handleResponse(const RestResponse& response, bool isSecondaryUrl)
{
    unsigned long lastTimestamp = millis();
    int statusCode = response.httpCode;

    // ===== Connection failure (statusCode <= 0) =====
    if (statusCode <= 0) {
        if (!isSecondaryUrl && strlen(Configuration.get().Tost.SecondUrl) > 0) {
            ESP_LOGW(TAG, "Primary URL connection failed, trying secondary URL");
            queueSecondaryUrlRequest();
            return;
        }
        ESP_LOGE(TAG, "Connection to server not possible (both URLs failed)");
        lastErrorMessage = "Connection to server not possible: " + response.body;
        lastErrorStatusCode = statusCode;
        lastErrorTimestamp = lastTimestamp;
        // Keep in queue, retry after 60s
        restTimeout.set(60 * 1000);
        return;
    }

    // ===== 2xx Success =====
    if (statusCode >= 200 && statusCode < 300) {
        lastSuccessfullyTimestamp = lastTimestamp;
        ESP_LOGI(TAG, "Successfully sent data (code=%d)", statusCode);
        // Pop queue and clear state
        if (!requestsToSend.empty()) {
            _queueMemoryBytes -= requestsToSend.front().length();
            requestsToSend.pop();
        }
        _visitedRedirectUrls.clear();
        restTimeout.set(0);
        return;
    }

    // ===== 3xx Redirect =====
    if (statusCode >= 300 && statusCode < 400) {
        const String& location = response.location;

        if (location.length() > 0) {
            // Only follow plain HTTP redirects — reject anything else (HTTPS, FTP, relative, etc.)
            String locationLower = location;
            locationLower.toLowerCase();
            if (!locationLower.startsWith("http://")) {
                ESP_LOGW(TAG, "Non-HTTP redirect refused (only http:// supported): %s", location.c_str());
                if (!isSecondaryUrl && strlen(Configuration.get().Tost.SecondUrl) > 0) {
                    ESP_LOGW(TAG, "Trying secondary URL instead");
                    queueSecondaryUrlRequest();
                    return;
                }
                // On secondary or no secondary configured — permanent failure for this request
                lastErrorMessage = "Non-HTTP redirect refused: " + location;
                lastErrorStatusCode = statusCode;
                lastErrorTimestamp = lastTimestamp;
                if (!requestsToSend.empty()) {
                    _queueMemoryBytes -= requestsToSend.front().length();
                    requestsToSend.pop();
                }
                _visitedRedirectUrls.clear();
                restTimeout.set(0);
                return;
            }

            // HTTP redirect — check for cycle or redirect limit
            if (_visitedRedirectUrls.count(location) > 0 || _visitedRedirectUrls.size() >= MAX_REDIRECTS) {
                if (_visitedRedirectUrls.count(location) > 0) {
                    ESP_LOGW(TAG, "Redirect cycle detected: %s", location.c_str());
                } else {
                    ESP_LOGW(TAG, "Redirect limit (%d) reached", MAX_REDIRECTS);
                }
                // Try secondary URL if on primary
                if (!isSecondaryUrl && strlen(Configuration.get().Tost.SecondUrl) > 0) {
                    ESP_LOGW(TAG, "Trying secondary URL instead");
                    _visitedRedirectUrls.clear();
                    queueSecondaryUrlRequest();
                    return;
                }
                // On secondary — discard request
                lastErrorMessage = "Redirect cycle/limit exceeded";
                lastErrorStatusCode = statusCode;
                lastErrorTimestamp = lastTimestamp;
                if (!requestsToSend.empty()) {
                    _queueMemoryBytes -= requestsToSend.front().length();
                    requestsToSend.pop();
                }
                _visitedRedirectUrls.clear();
                restTimeout.set(0);
                return;
            }

            // Follow the HTTP redirect with a fresh nonce
            ESP_LOGI(TAG, "Following HTTP redirect (code=%d) to: %s", statusCode, location.c_str());
            _visitedRedirectUrls.insert(location);
            sendToRedirectUrl(location, isSecondaryUrl);
            return;
        }

        // No Location header in 3xx — try secondary URL
        ESP_LOGW(TAG, "3xx response (code=%d) with no Location header", statusCode);
        if (!isSecondaryUrl && strlen(Configuration.get().Tost.SecondUrl) > 0) {
            ESP_LOGW(TAG, "Trying secondary URL instead");
            queueSecondaryUrlRequest();
            return;
        }
        // On secondary or no secondary — discard
        lastErrorMessage = String("Redirect with no Location header, code=") + String(statusCode);
        lastErrorStatusCode = statusCode;
        lastErrorTimestamp = lastTimestamp;
        if (!requestsToSend.empty()) {
            _queueMemoryBytes -= requestsToSend.front().length();
            requestsToSend.pop();
        }
        _visitedRedirectUrls.clear();
        restTimeout.set(0);
        return;
    }

    // ===== 4xx Client Error =====
    if (statusCode >= 400 && statusCode < 500) {
        ESP_LOGE(TAG, "Client error (code=%d) — permanent failure, not retrying", statusCode);
        lastErrorStatusCode = statusCode;
        lastErrorTimestamp = lastTimestamp;

        // Parse error message from response
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response.body);
        if (error || !doc["error"].is<String>()) {
            lastErrorMessage = String("Client error ") + String(statusCode) + ": " + response.body;
        } else {
            lastErrorMessage = doc["error"].as<String>();
        }

        // Pop queue — client errors are permanent (bad request, auth failure, etc.)
        if (!requestsToSend.empty()) {
            _queueMemoryBytes -= requestsToSend.front().length();
            requestsToSend.pop();
        }
        _visitedRedirectUrls.clear();
        restTimeout.set(0);
        return;
    }

    // ===== 5xx Server Error =====
    if (statusCode >= 500) {
        ESP_LOGE(TAG, "Server error (code=%d)", statusCode);
        if (!isSecondaryUrl && strlen(Configuration.get().Tost.SecondUrl) > 0) {
            ESP_LOGW(TAG, "Trying secondary URL after server error");
            queueSecondaryUrlRequest();
            return;
        }
        // On secondary or no secondary — keep in queue, retry after 60s
        lastErrorStatusCode = statusCode;
        lastErrorTimestamp = lastTimestamp;

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response.body);
        if (error || !doc["error"].is<String>()) {
            lastErrorMessage = String("Server error ") + String(statusCode) + ": " + response.body;
        } else {
            lastErrorMessage = doc["error"].as<String>();
        }

        ESP_LOGI(TAG, "Keeping request in queue, retrying in 60s");
        restTimeout.set(60 * 1000);
        return;
    }
}

void TostHandleClass::processActiveRequest()
{
    if (!_activeRequest.has_value()) {
        return;  // No active request
    }

    // Non-blocking check if ready
    if (_activeRequest->future.wait_for(0) == LightFuture<RestResponse>::Status::READY) {
        RestResponse response = _activeRequest->future.get();
        bool isSecondary = _activeRequest->isSecondaryUrl;
        _activeRequest.reset();  // Clear active request

        handleResponse(response, isSecondary);
    }
}

void TostHandleClass::sendNextRequest()
{
    if (requestsToSend.empty()) {
        return;
    }

    // Peek at next request from queue (DON'T pop yet - only pop on success)
    const String& body = requestsToSend.front();
    const char* systemId = Configuration.get().Tost.SystemId;

    // Encrypt body with ChaCha20-Poly1305, systemId as AAD
    String encryptedBody;
    char nonceHex[25];
    if (!encryptBody(body, systemId, encryptedBody, nonceHex)) {
        ESP_LOGE(TAG, "Encryption failed — dropping request, pausing 60s");
        _queueMemoryBytes -= body.length();
        requestsToSend.pop();
        restTimeout.set(60 * 1000);
        return;
    }

    // Build URL: http://<host>/api/solar/data/aes-gcm?systemId=<systemId>
    String path = "/api/solar/data/aes-gcm?systemId=";
    path += systemId;
    String url = buildUrl(Configuration.get().Tost.Url, path.c_str());

    // X-Nonce header only — clientToken is the encryption key, never transmitted
    std::map<String, String> headers;
    headers["X-Nonce"] = nonceHex;

    ESP_LOGD(TAG, "Sending encrypted request to: %s (queue: %d)", url.c_str(), requestsToSend.size());

    // Queue request to RestRequestHandler
    auto future = RestRequestHandler.queueRequestWithHeaders(
        url, "POST", encryptedBody, "application/octet-stream",
        headers, 0, 15000  // maxRetries=0, timeout=15s
    );

    // Store as active request
    _activeRequest = ActiveRequest{std::move(future), false};
}

void TostHandleClass::queueSecondaryUrlRequest()
{
    const char* systemId = Configuration.get().Tost.SystemId;

    // Reuse body from queue front (still there, not yet popped)
    const String& body = requestsToSend.front();

    // Encrypt body with ChaCha20-Poly1305, systemId as AAD
    String encryptedBody;
    char nonceHex[25];
    if (!encryptBody(body, systemId, encryptedBody, nonceHex)) {
        ESP_LOGE(TAG, "Encryption failed on secondary URL — skipping");
        restTimeout.set(60 * 1000);
        return;
    }

    // Build URL: http://<host>/api/solar/data/aes-gcm?systemId=<systemId>
    String path = "/api/solar/data/aes-gcm?systemId=";
    path += systemId;
    String url = buildUrl(Configuration.get().Tost.SecondUrl, path.c_str());

    // X-Nonce header only
    std::map<String, String> headers;
    headers["X-Nonce"] = nonceHex;

    ESP_LOGD(TAG, "Sending encrypted request to secondary URL: %s", url.c_str());

    auto future = RestRequestHandler.queueRequestWithHeaders(
        url, "POST", encryptedBody, "application/octet-stream",
        headers, 0, 10000  // 10s timeout for secondary
    );

    // Replace active request with secondary URL attempt
    _activeRequest = ActiveRequest{std::move(future), true};
}

void TostHandleClass::sendToRedirectUrl(const String& locationUrl, bool isSecondaryUrl)
{
    if (requestsToSend.empty()) {
        ESP_LOGW(TAG, "Queue empty during redirect follow — aborting");
        return;
    }

    const char* systemId = Configuration.get().Tost.SystemId;
    const String& body = requestsToSend.front();

    // Re-encrypt with fresh nonce for the redirect URL
    String encryptedBody;
    char nonceHex[25];
    if (!encryptBody(body, systemId, encryptedBody, nonceHex)) {
        ESP_LOGE(TAG, "Encryption failed during redirect follow — pausing 60s");
        restTimeout.set(60 * 1000);
        return;
    }

    // Append systemId query parameter if not already present in redirect URL
    String url = locationUrl;
    if (url.indexOf("systemId=") < 0) {
        if (url.indexOf('?') >= 0) {
            url += "&systemId=";
        } else {
            url += "?systemId=";
        }
        url += systemId;
    }

    // X-Nonce header only
    std::map<String, String> headers;
    headers["X-Nonce"] = nonceHex;

    ESP_LOGI(TAG, "Following redirect to: %s", url.c_str());

    auto future = RestRequestHandler.queueRequestWithHeaders(
        url, "POST", encryptedBody, "application/octet-stream",
        headers, 0, 15000  // 15s timeout
    );

    // Keep the same isSecondaryUrl flag — redirect is part of the same attempt
    _activeRequest = ActiveRequest{std::move(future), isSecondaryUrl};
}

#endif // TOST