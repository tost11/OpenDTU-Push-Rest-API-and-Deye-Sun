// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Thomas Basler and others
 */
#include "TostHandle.h"
#include "Configuration.h"
#include "Datastore.h"
#include "MessageOutput.h"
#include <Hoymiles.h>
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
    lastErrorMessage = "";

    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&TostHandleClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.setInterval(1 * TASK_SECOND);
    _loopTask.enable();
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
        //MessageOutput.println("tost skip");
        return;
    }

    //if (_lastPublish.occured()) {

    if(_cleanupCheck.occured()){
        MessageOutput.println("Run cleanup");
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
            MessageOutput.print("cleaned: ");
            MessageOutput.println(item.first.c_str());
            _lastPublishedInverters.erase(item.first);
        }
    }

    //DynamicJsonDocument data(4096);
    //data["duration"] = Configuration.get().Tost.Duration;
    //JsonArray devices = data.createNestedArray("devicses");

    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {

        auto inv = Hoymiles.getInverterByPos(i);
        if (inv->DevInfo()->getLastUpdate() <= 0) {
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

        uint32_t diff = lastUpdate - cachedLastUpdate;
        MessageOutput.printf("last: %d ",lastUpdate);
        MessageOutput.printf("calc: %d ",cachedLastUpdate);
        MessageOutput.printf("diff: %d\n",diff);
        if(cachedLastUpdate != 0 && diff < Configuration.get().Tost.Duration * 1000){
            //no update needed
            continue;
        }

        uint64_t id = inv->serial();

        MessageOutput.printf("-> New data to push for Inverter %llu\n\r", id);
        _lastPublishedInverters[uniqueID] = lastUpdate;

        DynamicJsonDocument data(4096);

        float duration = (float)diff / 1000;

        if(duration > Configuration.get().Tost.Duration * 1.2){
            duration = Configuration.get().Tost.Duration * 1.2;
        }

        data["duration"] = duration;

        auto devices = data.createNestedArray("devices");
        auto device = devices.createNestedObject();
        device["id"] = id;

        JsonArray inputs = device.createNestedArray("inputsDC");
        JsonArray outputs = device.createNestedArray("outputsAC");

        int inputCount = 0;
        int outputCount = 0;

        bool isData = false;

        // Loop all channels
        for (auto& channelType : inv->Statistics()->getChannelTypes()) {
            for (auto& c : inv->Statistics()->getChannelsByType(channelType)) {

                MessageOutput.printf("Next Channel: %d\n\r",channelType);

                if(channelType == 0){//inverter
                    isData = true;
                    DynamicJsonDocument output(256);
                    output["id"] = outputCount++;
                    output["voltage"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_UAC);
                    output["ampere"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_IAC);
                    output["watt"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_PAC);
                    output["frequency"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_F);
                    output["totalKWH"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_YT);
                    outputs.add(output);
                }else if(channelType == 1){
                    isData = true;
                    DynamicJsonDocument input(256);
                    input["id"] = inputCount++;
                    input["voltage"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_UDC);
                    input["ampere"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_IDC);
                    input["watt"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_PDC);
                    input["totalKWH"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_YT);
                    inputs.add(input);
                }else if(channelType == 2){
                    isData = true;
                    device["temperature"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_T);
                }

                /*for (uint8_t f = 0; f < sizeof(_publishFields) / sizeof(FieldId_t); f++) {
                    MessageOutput.printf("%d: %f\n\r",_publishFields[f],inv->Statistics()->getChannelFieldValue(channelType, c, _publishFields[f]));
                }*/
            }
        }

        if(isData){
            if(requestsToSend.size() < 10 ) {

                auto toSend = std::make_unique<http_requests_to_send>(millis());

                serializeJson(data, toSend->data);

                MessageOutput.println("adding new request to queue");
                requestsToSend.push(std::move(toSend));
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

    if(!_lastRequestResponse.has_value() && _currentlySendingData == nullptr && requestsToSend.size() > 0){
        //send new request
        MessageOutput.println("start new http request");
        //runNextHttpRequest(std::move(data));

        _currentlySendingData = std::move(requestsToSend.front());
        requestsToSend.pop();

        auto cfg = esp_pthread_get_default_config();
        cfg.thread_name = "other thread"; // adjust to name your thread
        cfg.stack_size = 8192; // adjust as needed
        esp_pthread_set_cfg(&cfg);
        _runningThread = std::thread(std::bind(&TostHandleClass::runNextHttpRequest,this));
        //runningHttpRequest = std::async(&TostHandleClass::runNextHttpRequest,this,std::move(data));
    }
}


void TostHandleClass::runNextHttpRequest() {

    MessageOutput.println("start thread");

    MessageOutput.println(_currentlySendingData->data);

    auto http = std::make_unique<HTTPClient>();

    std::string url = Configuration.get().Tost.Url;
    url+="/api/solar/data?systemId=";
    url+=Configuration.get().Tost.SystemId;

    http->begin(url.c_str());
    http->addHeader("clientToken",Configuration.get().Tost.Token);
    http->addHeader("Content-Type", "application/json");
    http->setTimeout(20 * 1000);//20 sec timout

    MessageOutput.println("Before post data");

    int statusCode = http->POST(_currentlySendingData->data);

    MessageOutput.println("Thread finished request");

    _lastRequestResponse = std::make_pair(statusCode,std::move(http));
    _currentlySendingData = nullptr;
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

            // ArduinoJson 6
            DynamicJsonDocument doc(512);
            // ArduinoJson 6
            DeserializationError error = deserializeJson(doc, payload);
            if (error){
                lastErrorMessage = std::string("Error on serializing response from Server. Data is: ")+payload.c_str();
            }else{
                if(!doc.containsKey("error")){
                    lastErrorMessage = std::string("Error json response missing 'error' key Data is: ")+payload.c_str();
                }else{
                    const char* err = doc["error"];
                    lastErrorMessage = err;
                }
            }
        }
    }
}
