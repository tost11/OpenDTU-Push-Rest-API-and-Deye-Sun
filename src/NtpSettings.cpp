// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 - 2023 Thomas Basler and others
 */
#include "NtpSettings.h"
#include "Configuration.h"
#include <Arduino.h>
#include <time.h>
#include "esp_sntp.h"
#include "MessageOutput.h"

void timeavailable(struct timeval *t) {
  NtpSettings.setTimeInSync(true);
}

NtpSettingsClass::NtpSettingsClass()
{
    _timeInSync = false;
}

void NtpSettingsClass::init()
{
    setServer();
    setTimezone();
    //callback if time is in sync
    sntp_set_time_sync_notification_cb(timeavailable);
}

void NtpSettingsClass::setServer()
{
    configTime(0, 0, Configuration.get().Ntp.Server);
}

void NtpSettingsClass::setTimezone()
{
    setenv("TZ", Configuration.get().Ntp.Timezone, 1);
    tzset();
}

void NtpSettingsClass::setTimeInSync(bool sync){
    _timeInSync = sync;
}


NtpSettingsClass NtpSettings;
