// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Thomas Basler and others
 */
#include "TostHandle.h"
#include "Configuration.h"
#include "Datastore.h"
#include "MessageOutput.h"
#include <Hoymiles.h>
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
    lastErrorStatusCode = 0;
    lastErrorTimestamp = 0;
    lastSuccessfullyTimestamp = 0;
    restTimeout.set(0);
    lastErrorMessage = "";

    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&TostHandleClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.setInterval(1 * TASK_SECOND);
    _loopTask.enable();
}

bool TostHandleClass::parseKWHValues(InverterAbstract * inv, JsonObject & doc, const ChannelType_t type, const ChannelNum_t channel) {
    bool changed = false;
    if(inv->Statistics()->hasChannelFieldValue(type, channel, FLD_YT)) {
        doc["totalKWH"] = inv->Statistics()->getChannelFieldValue(type, channel, FLD_YT) / (inv->Statistics()->getChannelFieldUnitId(type,channel,FLD_YT) == UNIT_WH ? 1000.f : 1.f);
        changed = true;
    }
    if(inv->Statistics()->hasChannelFieldValue(type, channel, FLD_YD)) {
        doc["dailyKWH"] = inv->Statistics()->getChannelFieldValue(type, channel, FLD_YD)  / (inv->Statistics()->getChannelFieldUnitId(type,channel,FLD_YD) == UNIT_WH ? 1000.f : 1.f);
        changed = true;
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

    if (!Configuration.get().Tost.Enabled || !Hoymiles.isAllRadioIdle()) {
        return;
    }

    // 1. Process active request (check if complete)
    processActiveRequest();

    // 2. If no active request, try to send next from queue
    if (!_activeRequest.has_value() && !requestsToSend.empty() && restTimeout.occured()) {
        sendNextRequest();
    }

    // 3. Run cleanup check (unchanged)
    if(_cleanupCheck.occured()){
        ESP_LOGD(TAG,"Run cleanup");
        _cleanupCheck.set(TIMER_CLEANUP);

        auto toClean = _lastPublishedInverters;

        for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {

            auto inv = Hoymiles.getInverterByPos(i);
            if (inv->DevInfo()->getLastUpdate() <= 0) {
                continue;
            }

            std::string uniqueId = inv->serialString().c_str();
            toClean.erase(uniqueId);
        }

        for (const auto &item: toClean){
            ESP_LOGD(TAG,"cleaned: %s",item.first.c_str());
            _lastPublishedInverters.erase(item.first);
        }
    }

    // 4. Collect inverter data and add to queue (existing logic)
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {

        auto inv = Hoymiles.getInverterByPos(i);
        if (inv->Statistics()->getLastUpdate() <= 0) {
            continue;
        }

        std::string uniqueID = inv->serialString().c_str();
        uint32_t cachedLastUpdate = 0;
        auto it = _lastPublishedInverters.find(uniqueID);
        if(it != _lastPublishedInverters.end()){
            cachedLastUpdate = it->second;
        }

        uint32_t lastUpdate = inv->Statistics()->getLastUpdate();

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

        ESP_LOGD(TAG,"last: %d ",lastUpdate);
        ESP_LOGD(TAG,"calc: %d ",cachedLastUpdate);
        ESP_LOGD(TAG,"diff: %d\n",diff);

        if(cachedLastUpdate != 0 && diff < Configuration.get().Tost.Duration * 1000){
            //no update needed
            continue;
        }

        uint64_t id = inv->serial();

        ESP_LOGI(TAG,"New data to push for Inverter %llu\n\r", id);
        _lastPublishedInverters[uniqueID] = lastUpdate;

        JsonDocument data;

        float duration = (float)diff / 1000;

        if(duration > Configuration.get().Tost.Duration * 1.2){
            duration = Configuration.get().Tost.Duration * 1.2f;
        }

        data["duration"] = duration;
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            time_t now;
            time(&now);
            data["timeUnit"] = "SECONDS";
            data["timestamp"] = time(&now);
            ESP_LOGD(TAG,"Time set on new inverter info manually %lu\n\r", time(&now));
        }

        JsonArray devices = data["devices"].to<JsonArray>();
        auto device = devices.add<JsonObject>();
        device["id"] = id;

        JsonArray inputs = device["inputsDC"].to<JsonArray>();
        JsonArray outputs = device["outputsAC"].to<JsonArray>();

        int inputCount = 0;
        int outputCount = 0;

        bool isData = false;

        // Loop all channels
        for (auto& channelType : inv->Statistics()->getChannelTypes()) {
            for (auto& c : inv->Statistics()->getChannelsByType(channelType)) {

                //MessageOutput.printf("Next Channel: %d\n\r",channelType);

                if(channelType == 0){//inverter
                    isData = true;
                    auto output = outputs.add<JsonObject>();
                    output["id"] = outputCount++;
                    output["voltage"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_UAC);
                    output["ampere"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_IAC);
                    output["watt"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_PAC);
                    output["frequency"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_F);
                    parseKWHValues(inv.get(),output,channelType,c);
                }else if(channelType == 1){
                    isData = true;
                    auto input = inputs.add<JsonObject>();
                    input["id"] = inputCount++;
                    input["voltage"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_UDC);
                    input["ampere"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_IDC);
                    input["watt"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_PDC);
                    parseKWHValues(inv.get(),input,channelType,c);
                }else if(channelType == 2){
                    if(inv->Statistics()->hasChannelFieldValue(channelType, c, FLD_T)) {
                        isData = true;
                        device["temperature"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_T);
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
            serializeJson(data, toSend);

            // If queue full, remove oldest
            if(requestsToSend.size() >= MAX_QUEUE_SIZE) {
                ESP_LOGW(TAG, "Request queue full (%d), dropping oldest", MAX_QUEUE_SIZE);
                requestsToSend.pop();
            }

            ESP_LOGD(TAG, "Adding new request to queue (size: %d)", requestsToSend.size() + 1);
            requestsToSend.push(std::make_unique<String>(std::move(toSend)));
        }
    }
}


void TostHandleClass::handleResponse(const RestResponse& response, bool isSecondaryUrl)
{
    unsigned long lastTimestamp = millis();
    int statusCode = response.httpCode;

    if (!response.success || statusCode <= 0) {
        // Connection failure - try secondary URL if not already tried
        if (!isSecondaryUrl && strlen(Configuration.get().Tost.SecondUrl) > 0) {
            ESP_LOGW(TAG, "First URL failed, trying secondary URL");
            queueSecondaryUrlRequest();
            return;  // Don't update error state yet
        }

        ESP_LOGE(TAG, "Tost's Solar Monitoring Error on rest call, connection to server not possible");
        lastErrorMessage = "Connection to server not possible: " + response.body;
        lastErrorStatusCode = statusCode;
        lastErrorTimestamp = lastTimestamp;
    } else {
        // Parse response body
        ESP_LOGD(TAG, "Full Status: %s", response.body.c_str());
        if (statusCode == 200) {
            lastSuccessfullyTimestamp = lastTimestamp;
            ESP_LOGI(TAG, "Tost's Solar Monitoring Successfully sent data");
        } else {
            lastErrorStatusCode = statusCode;
            lastErrorTimestamp = lastTimestamp;
            ESP_LOGE(TAG, "Tost's Solar Monitoring Error on rest call, Status code: %d", statusCode);

            // Parse error message from response
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response.body);
            if (error || !doc["error"].is<String>()) {
                lastErrorMessage = String("Error response: ") + response.body;
            } else {
                lastErrorMessage = doc["error"].as<String>();
            }
        }
    }

    // Pop from queue only if request was successful (following original logic)
    // Original: pop if statusCode > 0 AND statusCode != 403 AND statusCode != 401
    if (statusCode > 0 && statusCode != 403 && statusCode != 401 && statusCode != 503) {//on thes status codes a retry can be done
        // Check if front of queue is still the request we sent
        if (!requestsToSend.empty() && *requestsToSend.front() == _lastRequestBody) {
            ESP_LOGD(TAG, "Removing sent request from queue with code: %d, (succsfull nor not remove so not blocking other requests) queue remaining: %d", statusCode, requestsToSend.size() - 1);
            requestsToSend.pop();
        } else if (!requestsToSend.empty()) {
            ESP_LOGW(TAG, "Queue front changed during request - already removed by queue overflow");
        } else {
            ESP_LOGW(TAG, "Queue is empty - request already removed");
        }
        // Clear timeout on successful send
        restTimeout.set(0);
    } else {
        // Keep request in queue for retry, set timeout
        ESP_LOGI(TAG, "Request failed (code=%d), keeping in queue and pausing for 60s", statusCode);
        restTimeout.set(60 * 1000);
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
    String body = *requestsToSend.front();

    // Save for potential secondary URL retry and for matching later
    _lastRequestBody = body;

    // Build URL
    String url = Configuration.get().Tost.Url;
    url += "/api/solar/data?systemId=";
    url += Configuration.get().Tost.SystemId;

    // Prepare headers
    std::map<String, String> headers;
    headers["clientToken"] = Configuration.get().Tost.Token;

    ESP_LOGD(TAG, "Sending request to: %s (queue size: %d)", url.c_str(), requestsToSend.size());

    // Queue request to RestRequestHandler
    auto future = RestRequestHandler.queueRequestWithHeaders(
        url, "POST", body, "application/json",
        headers, 0, 15000  // maxRetries=0, timeout=15s
    );

    // Store as active request
    _activeRequest = ActiveRequest{std::move(future), false};
}

void TostHandleClass::queueSecondaryUrlRequest()
{
    String url = Configuration.get().Tost.SecondUrl;
    url += "/api/solar/data?systemId=";
    url += Configuration.get().Tost.SystemId;

    std::map<String, String> headers;
    headers["clientToken"] = Configuration.get().Tost.Token;

    ESP_LOGD(TAG, "Sending request to secondary URL: %s", url.c_str());

    // Reuse last JSON body
    auto future = RestRequestHandler.queueRequestWithHeaders(
        url, "POST", _lastRequestBody, "application/json",
        headers, 0, 10000  // 10s timeout for secondary
    );

    // Replace active request with secondary URL attempt
    _activeRequest = ActiveRequest{std::move(future), true};
}