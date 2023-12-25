#pragma once

#include "BaseSystemConfigPara.h"

class DeyeSystemConfigPara : public BaseSystemConfigPara {
public:
    float getLimitPercent() const override;

    LastCommandSuccess getLastLimitCommandSuccess() const override;
};
