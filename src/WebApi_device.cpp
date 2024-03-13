// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_device.h"
#include "Configuration.h"
#include "Display_Graphic.h"
#include "PinMapping.h"
#include "Utils.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "helper.h"
#include <AsyncJson.h>

void WebApiDeviceClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/device/config", HTTP_GET, std::bind(&WebApiDeviceClass::onDeviceAdminGet, this, _1));
    _server->on("/api/device/config", HTTP_POST, std::bind(&WebApiDeviceClass::onDeviceAdminPost, this, _1));
}

void WebApiDeviceClass::onDeviceAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse(false, MQTT_JSON_DOC_SIZE);
    auto& root = response->getRoot();
    const CONFIG_T& config = Configuration.get();
    const PinMapping_t& pin = PinMapping.get();

    auto curPin = root.createNestedObject("curPin");
    curPin["name"] = config.Dev_PinMapping;

    auto nrfPinObj = curPin.createNestedObject("nrf24");
    nrfPinObj["clk"] = pin.nrf24_clk;
    nrfPinObj["cs"] = pin.nrf24_cs;
    nrfPinObj["en"] = pin.nrf24_en;
    nrfPinObj["irq"] = pin.nrf24_irq;
    nrfPinObj["miso"] = pin.nrf24_miso;
    nrfPinObj["mosi"] = pin.nrf24_mosi;

    auto cmtPinObj = curPin.createNestedObject("cmt");
    cmtPinObj["clk"] = pin.cmt_clk;
    cmtPinObj["cs"] = pin.cmt_cs;
    cmtPinObj["fcs"] = pin.cmt_fcs;
    cmtPinObj["sdio"] = pin.cmt_sdio;
    cmtPinObj["gpio2"] = pin.cmt_gpio2;
    cmtPinObj["gpio3"] = pin.cmt_gpio3;

    auto ethPinObj = curPin.createNestedObject("eth");
    ethPinObj["enabled"] = pin.eth_enabled;
    ethPinObj["phy_addr"] = pin.eth_phy_addr;
    ethPinObj["power"] = pin.eth_power;
    ethPinObj["mdc"] = pin.eth_mdc;
    ethPinObj["mdio"] = pin.eth_mdio;
    ethPinObj["type"] = pin.eth_type;
    ethPinObj["clk_mode"] = pin.eth_clk_mode;

    auto displayPinObj = curPin.createNestedObject("display");
    displayPinObj["type"] = pin.display_type;
    displayPinObj["data"] = pin.display_data;
    displayPinObj["clk"] = pin.display_clk;
    displayPinObj["cs"] = pin.display_cs;
    displayPinObj["reset"] = pin.display_reset;

    auto servoPinObj = curPin.createNestedObject("servo");
    servoPinObj["pwm"] = pin.servo_pwm;

    auto ledPinObj = curPin.createNestedObject("led");
    for (uint8_t i = 0; i < PINMAPPING_LED_COUNT; i++) {
        ledPinObj["led" + String(i)] = pin.led[i];
    }

    auto display = root.createNestedObject("display");
    display["rotation"] = config.Display.Rotation;
    display["power_safe"] = config.Display.PowerSafe;
    display["screensaver"] = config.Display.ScreenSaver;
    display["contrast"] = config.Display.Contrast;
    display["language"] = config.Display.Language;
    display["diagramduration"] = config.Display.Diagram.Duration;
    display["diagrammode"] = config.Display.Diagram.Mode;

    auto leds = root.createNestedArray("led");
    for (uint8_t i = 0; i < PINMAPPING_LED_COUNT; i++) {
        auto led = leds.createNestedObject();
        led["brightness"] = config.Led_Single[i].Brightness;
    }

    auto servo = root.createNestedObject("servo");
    servo[F("frequency")] = config.Servo.Frequency;
    servo[F("resolution")] = config.Servo.Resolution;
    servo[F("range_min")] = config.Servo.RangeMin;
    servo[F("range_max")] = config.Servo.RangeMax;
    servo[F("serial")] = config.Servo.Serial;
    servo[F("input_index")] = config.Servo.InputIndex;
    servo[F("power")] = config.Servo.Power;

    response->setLength();
    request->send(response);
}

inline bool validateServoData(const JsonDocument & root,JsonVariant & retMsg){
    if (root["servo"][F("frequency")].as<int>() <= 0 || root["servo"][F("frequency")].as<int>() > 255) {
        retMsg[F("message")] = F("Frequency must be above 0 and below 256");
        retMsg[F("code")] = WebApiError::ServoFrequency;
        return false;
    }

    if (root["servo"][F("resolution")].as<int>() <= 0 || root["servo"][F("resolution")].as<int>() > 255) {
        retMsg[F("message")] = F("Resolution must be above 0 and below 256");
        retMsg[F("code")] = WebApiError::ServoResolution;
        return false;
    }

    if (root["servo"][F("range_min")].as<int>() <= 0 || root["servo"][F("range_min")].as<int>() >= 256) {
        retMsg[F("message")] = F("Pin must be above 0 and below 256");
        retMsg[F("code")] = WebApiError::ServoPin;
        return false;
    }

    if (root["servo"][F("range_max")].as<int>() <= 0 || root["servo"][F("range_max")].as<int>() >= 256) {
        retMsg[F("message")] = F("Servo Max must be above 0 and below 256");
        retMsg[F("code")] = WebApiError::ServoMax;
        return false;
    }

    if (root["servo"][F("range_max")].as<int>() == root["servo"][F("range_min")].as<int>()) {
        retMsg[F("message")] = F("Servo Max can not be equal to Servo Min");
        retMsg[F("code")] = WebApiError::ServoRange;
        return false;
    }

    if (root["servo"][F("input_index")].as<int>() < 0 || root["servo"][F("input_index")].as<int>() >= 256) {
        retMsg[F("message")] = F("Index Max must be above or equal 0 and below 256");
        retMsg[F("code")] = WebApiError::ServoIndex;
        return false;
    }

    if (root["servo"][F("power")].as<int>() <= 0 || root["servo"][F("power")].as<int>() >= 65535) {
        retMsg[F("message")] = F("Power Max must be above 0 and below 655345");
        retMsg[F("code")] = WebApiError::ServoPower;
        return false;
    }
    return true;
}

void WebApiDeviceClass::onDeviceAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse(false, MQTT_JSON_DOC_SIZE);
    auto& retMsg = response->getRoot();
    retMsg["type"] = "warning";

    if (!request->hasParam("data", true)) {
        retMsg["message"] = "No values found!";
        retMsg["code"] = WebApiError::GenericNoValueFound;
        response->setLength();
        request->send(response);
        return;
    }

    const String json = request->getParam("data", true)->value();

    if (json.length() > MQTT_JSON_DOC_SIZE) {
        retMsg["message"] = "Data too large!";
        retMsg["code"] = WebApiError::GenericDataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(MQTT_JSON_DOC_SIZE);
    const DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg["message"] = "Failed to parse data!";
        retMsg["code"] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("curPin")
            || root.containsKey("display")
            || root.containsKey("servo"))) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    if (root["curPin"]["name"].as<String>().length() == 0 || root["curPin"]["name"].as<String>().length() > DEV_MAX_MAPPING_NAME_STRLEN) {
        retMsg["message"] = "Pin mapping must between 1 and " STR(DEV_MAX_MAPPING_NAME_STRLEN) " characters long!";
        retMsg["code"] = WebApiError::HardwarePinMappingLength;
        retMsg["param"]["max"] = DEV_MAX_MAPPING_NAME_STRLEN;
        response->setLength();
        request->send(response);
        return;
    }

    if(root["curPin"]["servo"]["pwm"].as<int>() > 0){
        //validate servo data
        if(!validateServoData(root,retMsg)){
            response->setLength();
            request->send(response);
            return;
        }
    }

    CONFIG_T& config = Configuration.get();
    bool performRestart = root["curPin"]["name"].as<String>() != config.Dev_PinMapping;

    strlcpy(config.Dev_PinMapping, root["curPin"]["name"].as<String>().c_str(), sizeof(config.Dev_PinMapping));
    config.Display.Rotation = root["display"]["rotation"].as<uint8_t>();
    config.Display.PowerSafe = root["display"]["power_safe"].as<bool>();
    config.Display.ScreenSaver = root["display"]["screensaver"].as<bool>();
    config.Display.Contrast = root["display"]["contrast"].as<uint8_t>();
    config.Display.Language = root["display"]["language"].as<uint8_t>();
    config.Display.Diagram.Duration = root["display"]["diagramduration"].as<uint32_t>();
    config.Display.Diagram.Mode = root["display"]["diagrammode"].as<DiagramMode_t>();

    for (uint8_t i = 0; i < PINMAPPING_LED_COUNT; i++) {
        config.Led_Single[i].Brightness = root["led"][i]["brightness"].as<uint8_t>();
        config.Led_Single[i].Brightness = min<uint8_t>(100, config.Led_Single[i].Brightness);
    }

    Display.setOrientation(config.Display.Rotation);
    Display.enablePowerSafe = config.Display.PowerSafe;
    Display.enableScreensaver = config.Display.ScreenSaver;
    Display.setContrast(config.Display.Contrast);
    Display.setLanguage(config.Display.Language);
    Display.setDiagramMode(static_cast<DiagramMode_t>(config.Display.Diagram.Mode));
    Display.Diagram().updatePeriod();

    config.Servo.Frequency = root["servo"][F("frequency")].as<uint8_t>();
    config.Servo.Resolution = root["servo"][F("resolution")].as<uint8_t>();
    config.Servo.RangeMin = root["servo"][F("range_min")].as<uint8_t>();
    config.Servo.RangeMax = root["servo"][F("range_max")].as<uint8_t>();
    config.Servo.Serial = root["servo"][F("serial")].as<uint64_t>();
    config.Servo.InputIndex = root["servo"][F("input_index")].as<uint8_t>();
    config.Servo.Power = root["servo"][F("power")].as<uint16_t>();

    WebApi.writeConfig(retMsg);

    response->setLength();
    request->send(response);

    if (performRestart) {
        Utils::restartDtu();
    }
}
