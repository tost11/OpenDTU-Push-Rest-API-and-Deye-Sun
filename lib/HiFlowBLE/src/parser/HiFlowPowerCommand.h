#pragma once

#include <parser/BasePowerCommand.h>
#include <Parser.h>

/**
 * Minimal power command parser for HiFlow BLE inverters.
 * Since we're not implementing power control in the prototype,
 * this just satisfies the BaseInverter template requirement.
 */
class HiFlowPowerCommand : public Parser, public BasePowerCommand {
public:
    LastCommandSuccess getLastPowerCommandSuccess() const override {
        return _lastStatus;
    }

    void setLastPowerCommandSuccess(LastCommandSuccess status) {
        _lastStatus = status;
    }

private:
    LastCommandSuccess _lastStatus = CMD_OK;
};
