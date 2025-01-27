#pragma once

#include "BaseInverter.h"

#define MAX_NAME_HOST 32

template<class StatT,class DevT,class AlarmT,class PowerT>
class BaseNetworkInverter :public BaseInverter<StatT,DevT,AlarmT,PowerT> {
public:
    BaseNetworkInverter() = default;
    ~BaseNetworkInverter() = default;

    virtual void setHostnameOrIp(const char * hostOrIp)  = 0;
    virtual void setPort(uint16_t port) = 0;
};
