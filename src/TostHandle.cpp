// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Thomas Basler and others
 */
#include "TostHandle.h"
#include "Configuration.h"
#include "Datastore.h"
#include <MessageOutput.h>
#include "InverterHandler.h"
#include "NtpSettings.h"
#include <HTTPClient.h>
#include <ctime>
#include <ArduinoJson.h>
#include <esp_pthread.h>

TostHandleClass TostHandle;

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

bool TostHandleClass::parseKWHValues(BaseInverterClass * inv, JsonObject & doc, const ChannelType_t type, const ChannelNum_t channel) {
    bool changed = false;
    if(inv->getStatistics()->hasChannelFieldValue(type, channel, FLD_YT)) {
        doc["totalKWH"] = inv->getStatistics()->getChannelFieldValue(type, channel, FLD_YT) / (inv->getStatistics()->getChannelFieldUnitId(type,channel,FLD_YT) == UNIT_WH ? 1000.f : 1.f);
        changed = true;
    }
    if(inv->getStatistics()->hasChannelFieldValue(type, channel, FLD_YD)) {
        doc["dailyKWH"] = inv->getStatistics()->getChannelFieldValue(type, channel, FLD_YD)  / (inv->getStatistics()->getChannelFieldUnitId(type,channel,FLD_YD) == UNIT_WH ? 1000.f : 1.f);
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

    if (!Configuration.get().Tost.Enabled || !InverterHandler.isAllRadioIdle()) {
        //MessageOutput.println("tost skip");
        return;
    }

    //if (_lastPublish.occured()) {

    if(_cleanupCheck.occured()){
        MessageOutput.println("Run cleanup");
        _cleanupCheck.set(TIMER_CLEANUP);

        auto toClean = _lastPublishedInverters;

        for (uint8_t i = 0; i < InverterHandler.getNumInverters(); i++) {

            auto inv = InverterHandler.getInverterByPos(i);
            if (inv->getDevInfo()->getLastUpdate() <= 0) {
                continue;
            }

            std::string uniqueId = generateUniqueId(*inv);
            toClean.erase(uniqueId);
        }

        for (const auto &item: toClean){
            MessageOutput.print("cleaned: ");
            MessageOutput.println(item.first.c_str());
            _lastPublishedInverters.erase(item.first);
        }
    }

    for (uint8_t i = 0; i < InverterHandler.getNumInverters(); i++) {

        auto inv = InverterHandler.getInverterByPos(i);
        if (inv->getDevInfo()->getLastUpdate() <= 0) {
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

        uint32_t diff = lastUpdate - cachedLastUpdate;
        //MessageOutput.printf("last: %d ",lastUpdate);
        //MessageOutput.printf("calc: %d ",cachedLastUpdate);
        //MessageOutput.printf("diff: %d\n",diff);
        if(cachedLastUpdate != 0 && diff < Configuration.get().Tost.Duration * 1000){
            //no update needed
            continue;
        }

        uint64_t id = inv->serial();

        MessageOutput.printf("-> New data to push for Inverter %llu\n\r", id);
        _lastPublishedInverters[uniqueID] = lastUpdate;

        JsonDocument data;

        float duration = (float)diff / 1000;

        if(duration > Configuration.get().Tost.Duration * 1.2){
            duration = Configuration.get().Tost.Duration * 1.2;
        }

        data["duration"] = duration;
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            time_t now;
            time(&now);
            data["timeUnit"] = "SECONDS";
            if(NtpSettings.isTimeInSync()){
                data["timestamp"] = time(&now);
            }else{
                data["timestamp"] = 0;
            }
            MessageOutput.printf("Time set on new inverter info manually %lu\n\r", time(&now));
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
            if(requestsToSend.size() < 10 ) {

                String toSend;

                serializeJson(data, toSend);

                MessageOutput.println("adding new request to queue");
                requestsToSend.push(std::make_unique<String>(std::move(toSend)));
            }
        }
    }


    //check if last request ist send succesfully
    if(_lastRequestResponse.has_value()){
        MessageOutput.println("http request finished");
        _runningThread.join();
        handleResponse();
        _lastRequestResponse.reset();
    }

    if(!_lastRequestResponse.has_value() && _currentlySendingData == nullptr && requestsToSend.size() > 0 && restTimeout.occured()){
        //send new request
        MessageOutput.printf("start new http request send queue size is: %d\r\n",requestsToSend.size());
        //runNextHttpRequest(std::move(data));

        _currentlySendingData = requestsToSend.front().get();

        auto cfg = esp_pthread_get_default_config();
        cfg.thread_name = "other thread"; // adjust to name your thread
        cfg.stack_size = 8192; // adjust as needed
        esp_pthread_set_cfg(&cfg);
        _runningThread = std::thread(std::bind(&TostHandleClass::runNextHttpRequest,this));
        //runningHttpRequest = std::async(&TostHandleClass::runNextHttpRequest,this,std::move(data));
    }
}

int TostHandleClass::doRequest(String url,uint16_t timeout){
    auto http = std::make_unique<HTTPClient>();

    url+="/api/solar/data?systemId=";
    url+=Configuration.get().Tost.SystemId;
    MessageOutput.print("Send reqeust to: ");
    MessageOutput.println(url.c_str());

    http->begin(url.c_str());
    http->addHeader("clientToken",Configuration.get().Tost.Token);
    http->addHeader("Content-Type", "application/json");
    http->setTimeout(timeout);

    MessageOutput.println(String("Begin post data to: ") + url);

    int statusCode = http->POST(*_currentlySendingData);

    MessageOutput.println(String("Finished post data response: ") + statusCode);

    _lastRequestResponse = std::make_pair(statusCode,std::move(http));

    return statusCode;
}

void TostHandleClass::runNextHttpRequest() {

    MessageOutput.println("start reqeust thread");
    MessageOutput.printlnDebug(_currentlySendingData->c_str());

    int statusCode = doRequest(Configuration.get().Tost.Url,15 * 1000);//15 sec

    if((statusCode <= 0 || statusCode == 502) && strlen(Configuration.get().Tost.SecondUrl) > 0 ){

        MessageOutput.println("First post url not working try second one");

        statusCode = doRequest(Configuration.get().Tost.SecondUrl,10 * 1000);//10 sec
    }

    MessageOutput.println("Thread finished request");

    _currentlySendingData = nullptr;
}

std::string TostHandleClass::generateUniqueId(const BaseInverterClass &inv) {
    return (from_inverter_type(inv.getInverterType()) + inv.serialString()).c_str();
}

void TostHandleClass::handleResponse()
{
    int statusCode = _lastRequestResponse->first;

    unsigned long lastTimestamp = millis();
    //MessageOutput.printf("Timestamp: %ld\n\r",lastTimestamp);
    //MessageOutput.printf("Status code: %d\n\r",lastErrorStatusCode);
    if(statusCode <= 0){
        lastErrorMessage = "Connection to server not possible";
        lastErrorStatusCode = statusCode;
        lastErrorTimestamp = lastTimestamp;
    }else{
        String payload = _lastRequestResponse->second->getString();
        //MessageOutput.printf("Full Status: %s\n\r", payload.c_str());
        if (statusCode == 200) {
            lastSuccessfullyTimestamp = lastTimestamp;
        }else {
            lastErrorMessage == payload.c_str();
            lastErrorStatusCode = statusCode;
            lastErrorTimestamp = lastTimestamp;
            MessageOutput.printf("Tost's Solar Monitoring Error on rest call: %s\n\r",lastErrorMessage.c_str());

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (error){
                lastErrorMessage = String("Error on serializing response from Server. Data is: ") + payload;
            }else{
                if(!doc["error"].is<String>()){
                    lastErrorMessage = String("Error json response missing 'error' key Data is: ") + payload;
                }else{
                    const char* err = doc["error"];
                    lastErrorMessage = err;
                }
            }
        }
    }


    if(statusCode > 0 && statusCode != 403 && statusCode != 401){
        //clear if not connection error or forbidden (bad request/internal server error => ok, because of mostly data error)
        requestsToSend.pop();
    }else{
        MessageOutput.println("Tost's Solar Monitoring use rest send pause (1 min) because last request failed, queue is\n\r");
        restTimeout.set(60 * 1000);

    }
}
