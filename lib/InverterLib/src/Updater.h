#pragma once

#include <cstdint>

class Updater {
public:
    uint32_t getLastUpdate() const;
    void setLastUpdate(const uint32_t lastUpdate);
private:
    uint32_t _lastUpdate = 0;
};
