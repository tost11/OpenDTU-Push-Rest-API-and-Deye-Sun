// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <string>
#include <WString.h>

class InverterUtils {
public:
    static uint8_t getWeekDay();
    static bool getTimeAvailable();
    static String dumpArray(const uint8_t buf[], const uint8_t len);
    static std::unique_ptr<std::unordered_map<std::string,std::string>> getConnectedClients(); 
};