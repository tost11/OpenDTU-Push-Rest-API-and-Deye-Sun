#pragma once

#include "BaseSystemConfigPara.h"

class HoymilesWSystemConfigPara : public BaseSystemConfigPara {
public:
    float getLimitPercent() const override;

    LastCommandSuccess getLastLimitCommandSuccess() const override;
};
