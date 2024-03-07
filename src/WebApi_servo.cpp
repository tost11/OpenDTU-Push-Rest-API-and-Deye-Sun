// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */

#include "WebApi_servo.h"
#include "Configuration.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include <AsyncJson.h>
#include "MessageOutput.h"

void WebApiServoClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/servo/config", HTTP_GET, std::bind(&WebApiServoClass::onServoAdminGet, this, _1));
    _server->on("/api/servo/config", HTTP_POST, std::bind(&WebApiServoClass::onServoAdminPost, this, _1));
}

void WebApiServoClass::loop()
{
}

void WebApiServoClass::onServoAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse(false, SERVO_JSON_DOC_SIZE);
    JsonObject root = response->getRoot();
    const CONFIG_T& config = Configuration.get();

    root[F("enabled")] = config.Servo.Enabled;
    root[F("frequency")] = config.Servo.Frequency;
    root[F("range_min")] = config.Servo.RangeMin;
    root[F("range_max")] = config.Servo.RangeMax;
    root[F("pin")] = config.Servo.Pin;
    root[F("serial")] = config.Servo.Serial;
    root[F("input_index")] = config.Servo.InputIndex;
    root[F("power")] = config.Servo.Power;

    response->setLength();
    request->send(response);
}

void WebApiServoClass::onServoAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse(false, SERVO_JSON_DOC_SIZE);
    JsonVariant retMsg = response->getRoot();
    retMsg[F("type")] = F("warning");

    if (!request->hasParam("data", true)) {
        retMsg[F("message")] = F("No values found!");
        retMsg[F("code")] = WebApiError::GenericNoValueFound;
        response->setLength();
        request->send(response);
        return;
    }

    String json = request->getParam("data", true)->value();

    if (json.length() > SERVO_JSON_DOC_SIZE) {
        retMsg[F("message")] = F("Data too large!");
        retMsg[F("code")] = WebApiError::GenericDataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(SERVO_JSON_DOC_SIZE);
    DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg[F("message")] = F("Failed to parse data!");
        retMsg[F("code")] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("enabled")
          && root.containsKey("frequency")
          && root.containsKey("pin")
          && root.containsKey("range_min")
          && root.containsKey("range_max")
          && root.containsKey("serial")
          && root.containsKey("input_index")
          && root.containsKey("power")
          )) {
        retMsg[F("message")] = F("Values are missing!");
        retMsg[F("code")] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    if (root[F("enabled")].as<bool>()) {
        if (root[F("frequency")].as<int>() <= 0 || root[F("frequency")].as<int>() > 255) {
            retMsg[F("message")] = F("Frequency must be above 0 and below 256");
            retMsg[F("code")] = WebApiError::ServoFrequency;
            response->setLength();
            request->send(response);
            return;
        }

        if (root[F("pin")].as<int>() <= 0 || root[F("pin")].as<int>() >= 256) {
            retMsg[F("message")] = F("Pin must be above 0 and below 256");
            retMsg[F("code")] = WebApiError::ServoPin;
            response->setLength();
            request->send(response);
            return;
        }

        //TODO check pin already in use

        if (root[F("range_min")].as<int>() <= 0 || root[F("range_min")].as<int>() >= 256) {
            retMsg[F("message")] = F("Pin must be above 0 and below 256");
            retMsg[F("code")] = WebApiError::ServoPin;
            response->setLength();
            request->send(response);
            return;
        }

        if (root[F("range_max")].as<int>() <= 0 || root[F("range_max")].as<int>() >= 256) {
            retMsg[F("message")] = F("Servo Max must be above 0 and below 256");
            retMsg[F("code")] = WebApiError::ServoMax;
            response->setLength();
            request->send(response);
            return;
        }

        if (root[F("range_max")].as<int>() == root[F("range_min")].as<int>()) {
            retMsg[F("message")] = F("Servo Max can not be equal to Servo Min");
            retMsg[F("code")] = WebApiError::ServoRange;
            response->setLength();
            request->send(response);
            return;
        }

        if (root[F("input_index")].as<int>() < 0 || root[F("input_index")].as<int>() >= 256) {
            retMsg[F("message")] = F("Index Max must be above or equal 0 and below 256");
            retMsg[F("code")] = WebApiError::ServoIndex;
            response->setLength();
            request->send(response);
            return;
        }

        if (root[F("power")].as<int>() <= 0 || root[F("power")].as<int>() >= 65535) {
            retMsg[F("message")] = F("Power Max must be above 0 and below 655345");
            retMsg[F("code")] = WebApiError::ServoPower;
            response->setLength();
            request->send(response);
            return;
        }
    }

    CONFIG_T& config = Configuration.get();
    config.Servo.Enabled = root[F("enabled")].as<bool>();
    config.Servo.Pin = root[F("pin")].as<uint8_t>();
    config.Servo.Frequency = root[F("frequency")].as<uint8_t>();
    config.Servo.RangeMin = root[F("range_min")].as<uint8_t>();
    config.Servo.RangeMax = root[F("range_max")].as<uint8_t>();
    config.Servo.Serial = root[F("serial")].as<uint64_t>();
    config.Servo.InputIndex = root[F("input_index")].as<uint8_t>();
    config.Servo.Power = root[F("power")].as<uint16_t>();

    WebApi.writeConfig(retMsg);

    response->setLength();
    request->send(response);

    //MqttSettings.performReconnect();
    //MqttHandleHass.forceUpdate();
}