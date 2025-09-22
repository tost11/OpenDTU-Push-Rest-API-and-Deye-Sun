// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023-2025 Thomas Basler and others
 */
#include "InverterUtils.h"
#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#include <esp_wifi.h>

uint8_t InverterUtils::getWeekDay()
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    return tm.tm_mday;
}

bool InverterUtils::getTimeAvailable()
{
    struct tm timeinfo;
    return getLocalTime(&timeinfo, 5);
}

String InverterUtils::dumpArray(const uint8_t data[], const uint8_t len)
{
    if (len == 0) {
        return String();
    }

    // Each byte needs 2 hex chars + 1 space (except last byte)
    String result;
    result.reserve(len * 3);

    char buf[4]; // Buffer for single hex value + space + null
    for (uint8_t i = 0; i < len; i++) {
        snprintf(buf, sizeof(buf), "%02X%s", data[i], (i < len - 1) ? " " : "");
        result += buf;
    }

    return result;
}


std::unique_ptr<std::unordered_map<std::string,std::string>> InverterUtils::getConnectedClients(){
    auto ret = std::make_unique<std::unordered_map<std::string,std::string>>();
    if(WiFi.softAPgetStationNum() > 0){
        //collectmore wifi info
        wifi_sta_list_t wifi_sta_list;
        tcpip_adapter_sta_list_t adapter_sta_list;

        memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
        memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

        esp_wifi_ap_get_sta_list(&wifi_sta_list);
        tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

        char ipBuff[16];
        char macBuff[18];

        for (int i = 0; i < adapter_sta_list.num; i++) {

            memset(ipBuff,0,16);
            memset(macBuff,0,18);
            macBuff[17] = '\0';

            const tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];


            for(int i = 0; i< 6; i++){
                sprintf(macBuff+i*3,"%02X", station.mac[i]);
                if(i<5){
                    sprintf(macBuff+2+i*3,":");
                }
            }

            memset(ipBuff,0,16);
            esp_ip4addr_ntoa(&(station.ip),ipBuff,16);

            ret->emplace(std::string(macBuff),std::string(ipBuff));
        }
    }
    return std::move(ret);
}
