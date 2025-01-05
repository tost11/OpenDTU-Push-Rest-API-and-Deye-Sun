#pragma once

#include "BaseGridProfile.h"

class HoymilesWGridProfile : public BaseGridProfile {
public:
    std::vector<uint8_t> getRawData() const override;
};
