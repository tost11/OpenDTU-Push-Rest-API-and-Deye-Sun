// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <cstdint>

#define INVERTER_UPDATE_SETTINGS_INTERVAL 60000l

class InverterSettingsClass {
public:
    InverterSettingsClass();
    void init(Scheduler& scheduler);

private:
    void settingsLoop();
    void hoyLoop();

    Task _settingsTask;

    #ifdef HOYMILES
    Task _hoyTask;
    void hoyLoop();
    #endif

    #ifdef DEYE_SUN
    Task _deyeTask;
    void deyeLoop();
    #endif

    #ifdef HOYMILESW
    Task _hoyWTask;
    void hoyWLoop();
    #endif
};

extern InverterSettingsClass InverterSettings;
