// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_devinfo.h"
#include "WebApi.h"
#include <AsyncJson.h>
#include <InverterHandler.h>
#include <ctime>

void WebApiDevInfoClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/devinfo/status", HTTP_GET, std::bind(&WebApiDevInfoClass::onDevInfoStatus, this, _1));
}

void WebApiDevInfoClass::onDevInfoStatus(AsyncWebServerRequest* request)
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
        root["valid_data"] = inv->DevInfo()->getLastUpdate() > 0;
        root["fw_bootloader_version"] = inv->DevInfo()->getFwBootloaderVersion();
        root["fw_build_version"] = inv->DevInfo()->getFwBuildVersion();
        root["hw_part_number"] = inv->DevInfo()->getHwPartNumber();
        root["hw_version"] = inv->DevInfo()->getHwVersion();
        root["hw_model_name"] = inv->DevInfo()->getHwModelName();
        root["max_power"] = inv->DevInfo()->getMaxPower();
        root["fw_build_datetime"] = inv->DevInfo()->getFwBuildDateTimeStr();
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}
