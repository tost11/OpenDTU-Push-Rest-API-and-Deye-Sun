// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <Arduino.h>
#include <cstdint>
#include <defines.h>

#define HOY_SEMAPHORE_TAKE() \
    do {                     \
    } while (xSemaphoreTake(_xSemaphore, portMAX_DELAY) != pdPASS)
#define HOY_SEMAPHORE_GIVE() xSemaphoreGive(_xSemaphore)

class Parser {
public:
    Parser();

    void beginAppendFragment();
    void endAppendFragment();

protected:
    SemaphoreHandle_t _xSemaphore;

};