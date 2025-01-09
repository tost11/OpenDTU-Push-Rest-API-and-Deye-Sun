// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

class NtpSettingsClass {
private:
    bool _timeInSync;
public:
    NtpSettingsClass();
    void init();

    void setServer();
    void setTimezone();

    bool isTimeInSync()const{return _timeInSync;}
    void setTimeInSync(bool sync);
};

extern NtpSettingsClass NtpSettings;
