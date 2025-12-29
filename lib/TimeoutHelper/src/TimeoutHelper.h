// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

class TimeoutHelper {
public:
    TimeoutHelper();
    TimeoutHelper(const uint32_t ms);
    void set(const uint32_t ms);
    void setTimeout(const uint32_t ms);
    void extend(const uint32_t ms);
    void reset();
    bool occured() const;
    uint32_t currentMillis() const;
    uint32_t dist() const;
    void zero();
    void print();
private:
    uint32_t startMillis;
    uint32_t timeout;
};