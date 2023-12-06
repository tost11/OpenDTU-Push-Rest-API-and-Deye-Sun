#pragma once

#include "BaseSystemConfigPara.h"

class DeyeSystemConfigPara : public BaseSystemConfigPara {
public:
    float getLimitPercent() override;

    LastCommandSuccess getLastLimitCommandSuccess() override;
};
