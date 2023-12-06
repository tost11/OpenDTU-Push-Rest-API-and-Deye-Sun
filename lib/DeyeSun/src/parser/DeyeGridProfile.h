#pragma once

#include "BaseGridProfile.h"

class DeyeGridProfile : public BaseGridProfile {
public:
    std::vector<uint8_t> getRawData() override;
};
