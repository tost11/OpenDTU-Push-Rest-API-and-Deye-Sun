// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Thomas Basler and others
 */
#include "InverterUtils.h"
#include <time.h>
#include <esp_wifi.h>
#include <WiFi.h>

uint8_t InverterUtils::getWeekDay()
{
    time_t raw;
    struct tm info;
    time(&raw);
    localtime_r(&raw, &info);
    return info.tm_mday;
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