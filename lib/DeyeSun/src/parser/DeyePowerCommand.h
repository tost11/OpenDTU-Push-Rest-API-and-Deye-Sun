#pragma once

#include "BasePowerCommand.h"

class DeyePowerCommand : public BasePowerCommand {
public:
    LastCommandSuccess getLastPowerCommandSuccess() const override;
};
