// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Thomas Basler and others
 */
#include "TostHandle.h"
#include "Configuration.h"
#include "Datastore.h"
#include <Hoymiles.h>
#include "MessageOutput.h"
#include <HTTPClient.h>
#include <ctime>
#include <ArduinoJson.h>

TostHandleClass TostHandle;

void TostHandleClass::init()
{
    _lastPublish.set(Configuration.get().Tost_Duration * 1000);
    lastErrorStatusCode = 0;
    lastTimestamp = 0;
    lastSuccessfullyTimestamp = 0;
    lastErrorMessage = "";
}

void TostHandleClass::loop()
{

    if (Configuration.get().Tost_Enabled == false || !Hoymiles.isAllRadioIdle()) {
        MessageOutput.println("tost skip");
        return;
    }

    if (_lastPublish.occured()) {

        MessageOutput.println("tost timer");


        HTTPClient http;

        // Your Domain name with URL path or IP address with path
        http.begin("http://192.168.66.249:8050/api/solar/data?system=whatever");
        //http.begin("https://solar.pihost.org");

        // If your need Node-RED/server authentication, insert user and password below
        //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

        // Send HTTP GET request
        int statusCode = http.GET();
        lastTimestamp = millis();
        MessageOutput.printf("Status code: %d\n\r",lastErrorStatusCode);
        if(statusCode <= 0){
            lastErrorMessage = "Connection to server not possible";
            lastErrorStatusCode = statusCode;
        }else{
            String payload = http.getString();
            MessageOutput.printf("Full Status: %s\n\r", payload.c_str());
            if (statusCode == 200) {
                lastSuccessfullyTimestamp = lastTimestamp;
            }else {
                lastErrorMessage == payload.c_str();
                lastErrorStatusCode = statusCode;
                MessageOutput.printf("Error: %s\n\r",lastErrorMessage.c_str());

                // ArduinoJson 6
                DynamicJsonDocument doc(1024);
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


        /*char recvBuff[1024*40];

        HTTPClient http;
        int ret = http.get("http://192.168.66.249:8050", recvBuff, sizeof(recvBuff),HTTP_CLIENT_DEFAULT_TIMEOUT);
        if (!ret) {
           MessageOutput.println("Result: %s\n\r", recvBuff);
        } else {
           MessageOutput.println("Error - ret = %d - HTTP return code = %d\n\r", ret, http.getHTTPResponseCode());
        }*/

        /*MqttSettings.publish("ac/power", String(Datastore.getTotalAcPowerEnabled(), Datastore.getTotalAcPowerDigits()));
        MqttSettings.publish("ac/yieldtotal", String(Datastore.getTotalAcYieldTotalEnabled(), Datastore.getTotalAcYieldTotalDigits()));
        MqttSettings.publish("ac/yieldday", String(Datastore.getTotalAcYieldDayEnabled(), Datastore.getTotalAcYieldDayDigits()));
        MqttSettings.publish("ac/is_valid", String(Datastore.getIsAllEnabledReachable()));
        MqttSettings.publish("dc/power", String(Datastore.getTotalDcPowerEnabled(), Datastore.getTotalDcPowerDigits()));
        MqttSettings.publish("dc/irradiation", String(Datastore.getTotalDcIrradiation(), 3));
        MqttSettings.publish("dc/is_valid", String(Datastore.getIsAllEnabledReachable()));*/

        _lastPublish.set(Configuration.get().Tost_Duration * 1000);
    }
}
