#pragma once

#include "BaseInverter.h"
#include <InverterUtils.h>
#include <TimeoutHelper.h>
#include <MessageOutput.h>

#define MAX_NAME_HOST 32

template<class StatT,class DevT,class AlarmT,class PowerT>
class BaseNetworkInverter :public BaseInverter<StatT,DevT,AlarmT,PowerT> {
private:
    static const uint32_t TIMER_SUCCES_MAC_IP_RESOLUTION = 5 * 60 * 1000;
    static const uint32_t TIMER_FAILED_MAC_IP_RESOLUTION = 30 * 1000;
    TimeoutHelper _macToIpResolverTimer;

    void checkIfIpOrHostnameIsMac() {
        const char* mac = _oringalIpOrHostname.c_str();
        int i = 0;
        int s = 0;
        while (*mac) {
        if (isxdigit(*mac)) {
            i++;
        }
        else if (*mac == ':' || *mac == '-') {

            if (i == 0 || i / 2 - 1 != s)
                break;

            ++s;
        }
        else {
            s = -1;
        }
        ++mac;
        }
        _IpOrHostnameIsMac = (i == 12 && s == 5 );
        MessageOutput.printf("Check if ip ist Mac: %s\n",_IpOrHostnameIsMac ? "true" : "false");
    }

public:
    BaseNetworkInverter():
    _IpOrHostnameIsMac(false),
    _resolvedIpByMacAdress(nullptr){
        _macToIpResolverTimer.set(TIMER_FAILED_MAC_IP_RESOLUTION);
        _macToIpResolverTimer.zero();
    }
    ~BaseNetworkInverter() = default;

    virtual void setHostnameOrIpOrMac(const char * ip){
        _oringalIpOrHostname = String(ip);
        checkIfIpOrHostnameIsMac();
        setHostnameOrIp(ip);
    }

    virtual void setPort(uint16_t port) = 0;
protected:
    virtual void setHostnameOrIp(const char * hostOrIp) = 0;

    std::unique_ptr<std::string> _resolvedIpByMacAdress;
    bool _IpOrHostnameIsMac;
    String _oringalIpOrHostname;

    bool checkForMacResolution(){
        if(_IpOrHostnameIsMac){
            if(_macToIpResolverTimer.occured()){
                MessageOutput.printfDebug("Try to resolve Mac to ip: %s\n",_oringalIpOrHostname.c_str());
                auto apMacsAndIps = InverterUtils::getConnectedClients();
                auto ip = String(_oringalIpOrHostname.c_str());
                ip.toUpperCase();
                auto found = apMacsAndIps->find(std::string(ip.c_str()));
                if(found != apMacsAndIps->end()){
                    if(!(found->second == "0.0.0.0" || found->second == "" || found->second == *_resolvedIpByMacAdress)){
                        _resolvedIpByMacAdress = std::make_unique<std::string>(found->second);
                        _macToIpResolverTimer.set(TIMER_SUCCES_MAC_IP_RESOLUTION);
                        MessageOutput.printf("Resolved Map to ip: %s\n",_resolvedIpByMacAdress->c_str());
                        return true;
                    }
                }
                _macToIpResolverTimer.set(TIMER_FAILED_MAC_IP_RESOLUTION);
                MessageOutput.printfDebug("Failed on reloving Mac to Ip");
                return false;
            }
        }
        return false;
    }


};
