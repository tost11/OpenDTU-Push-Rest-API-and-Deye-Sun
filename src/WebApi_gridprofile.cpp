// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2025 Thomas Basler and others
 */
#include "WebApi_gridprofile.h"
#include "WebApi.h"
#include <AsyncJson.h>
#include <InverterHandler.h>

void WebApiGridProfileClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/gridprofile/status", HTTP_GET, std::bind(&WebApiGridProfileClass::onGridProfileStatus, this, _1));
    server.on("/api/gridprofile/rawdata", HTTP_GET, std::bind(&WebApiGridProfileClass::onGridProfileRawdata, this, _1));
}

void WebApiGridProfileClass::onGridProfileStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    auto serial = WebApi.parseSerialFromRequest(request);
    auto inv = InverterHandler.getInverterBySerial(serial);

    if (inv != nullptr) {
        root["name"] = inv->getGridProfileParser()->getProfileName();
        root["version"] = inv->getGridProfileParser()->getProfileVersion();

        auto jsonSections = root["sections"].to<JsonArray>();
        auto profSections = inv->getGridProfileParser()->getProfile();

        for (auto& profSection : profSections) {
            auto jsonSection = jsonSections.add<JsonObject>();
            jsonSection["name"] = profSection.SectionName;

            auto jsonItems = jsonSection["items"].to<JsonArray>();

            for (auto& profItem : profSection.items) {
                auto jsonItem = jsonItems.add<JsonObject>();

                jsonItem["n"] = profItem.Name;
                jsonItem["u"] = profItem.Unit;
                jsonItem["v"] = profItem.Value;
            }
        }
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiGridProfileClass::onGridProfileRawdata(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    auto serial = WebApi.parseSerialFromRequest(request);

    if(!request->hasParam("manufacturer")){
        response->setCode(400);
        response->setLength();
        request->send(response);
        return;
    }

    auto type = to_inverter_type(request->getParam("manufacturer")->value());

    if(type == inverter_type::Inverter_count){
        response->setCode(400);
        response->setLength();
        request->send(response);
        return;
    }

    auto inv = InverterHandler.getInverterBySerial(serial,type);

    if (inv != nullptr) {
        auto raw = root["raw"].to<JsonArray>();
        auto data = inv->getGridProfileParser()->getRawData();

        copyArray(&data[0], data.size(), raw);
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}
