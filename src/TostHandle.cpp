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
#include <Hoymiles.h>

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

    if (!Configuration.get().Tost_Enabled || !Hoymiles.isAllRadioIdle()) {
        //MessageOutput.println("tost skip");
        return;
    }

    if (_lastPublish.occured()) {

        MessageOutput.println("tost timer");

        DynamicJsonDocument data(2048);
        data["duration"] = 10;
        JsonArray devices = data.createNestedArray("devices");

        for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {

            auto inv = Hoymiles.getInverterByPos(i);
            if (inv->DevInfo()->getLastUpdate() <= 0) {
                continue;
            }

            int id = inv->DevInfo()->getHwPartNumber();

            DynamicJsonDocument device(1024);
            device["id"] = id;

            JsonArray inputs = device.createNestedArray("inputsDC");
            JsonArray outputs = device.createNestedArray("outputsAC");

            MessageOutput.printf("-> Inverter %d\n\r", id);

            uint32_t lastUpdate = inv->Statistics()->getLastUpdate();

            int inputCount = 0;
            int outputCount = 0;
            if (lastUpdate > 0) {
                //_lastPublishStats[i] = lastUpdate;

                // Loop all channels
                for (auto& channelType : inv->Statistics()->getChannelTypes()) {
                    for (auto& c : inv->Statistics()->getChannelsByType(channelType)) {

                        MessageOutput.printf("Next Channel: %d\n\r",channelType);

                        if(channelType == 0){//inverter
                            DynamicJsonDocument output(256);
                            output["id"] = outputCount++;
                            output["voltage"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_UAC);
                            output["ampere"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_IAC);
                            output["watt"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_PAC);
                            output["frequency"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_F);
                            output["totalKWH"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_YT);
                            outputs.add(output);
                        }else if(channelType == 1){
                            DynamicJsonDocument input(256);
                            input["id"] = inputCount++;
                            input["voltage"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_UDC);
                            input["ampere"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_IDC);
                            input["watt"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_PDC);
                            input["totalKWH"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_YT);
                            inputs.add(input);
                        }else if(channelType == 2){
                            device["temperature"] = inv->Statistics()->getChannelFieldValue(channelType, c, FLD_F);
                        }

                        for (uint8_t f = 0; f < sizeof(_publishFields) / sizeof(FieldId_t); f++) {
                            MessageOutput.printf("%d: %f\n\r",_publishFields[f],inv->Statistics()->getChannelFieldValue(channelType, c, _publishFields[f]));

                        }
                    }
                }
            }

            devices.add(device);

        }

        String output;
        serializeJson(data, output);
        MessageOutput.println(output.c_str());


        HTTPClient http;

        std::string url = Configuration.get().Tost_Url;
        url+="/api/solar/data?systemId=";
        url+=Configuration.get().Tost_System_Id;


        http.begin(url.c_str());
        http.addHeader("clientToken",Configuration.get().Tost_Token);
        http.addHeader("Content-Type", "application/json");

        MessageOutput.printf("Request: %s\n\r",url.c_str());
        MessageOutput.printf("Token: %s\n\r",Configuration.get().Tost_Token);

        int statusCode = http.POST(output);
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
