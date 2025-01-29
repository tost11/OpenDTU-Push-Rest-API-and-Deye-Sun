// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <string>

class InverterUtils {
public:
    static uint8_t getWeekDay();
    static std::unique_ptr<std::unordered_map<std::string,std::string>> getConnectedClients(); 
};