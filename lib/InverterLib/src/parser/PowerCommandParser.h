// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <Parser.h>
#include <parser/BasePowerCommand.h>

class PowerCommandParser : public Parser, public BasePowerCommand {
public:
    void setLastPowerCommandSuccess(const LastCommandSuccess status);
    LastCommandSuccess getLastPowerCommandSuccess() const override;
    uint32_t getLastUpdateCommand() const;
    void setLastUpdateCommand(const uint32_t lastUpdate);

private:
    LastCommandSuccess _lastLimitCommandSuccess = CMD_OK; // Set to OK because we have to assume nothing is done at startup

    uint32_t _lastUpdateCommand = 0;
};