#pragma once

#include "BaseInverter.h"
#include <InverterUtils.h>
#include <TimeoutHelper.h>

template<class StatT,class DevT,class AlarmT,class PowerT>
class BaseNetworkInverter :public BaseInverter<StatT,DevT,AlarmT,PowerT> {
private:
    static const uint32_t TIMER_SUCCESS_MAC_IP_RESOLUTION = 5 * 60 * 1000;
    static const uint32_t TIMER_FAILED_MAC_IP_RESOLUTION = 30 * 1000;
    static const uint32_t TIMER_NOT_FOUND_RESET_MAC_IP_RESOLUTION = 15 * 60 * 1000;
    TimeoutHelper _macToIpResolverTimer;
    TimeoutHelper _macUnknownTimer;

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
        ESP_LOGI(LogTag().c_str(),"Check if ip ist Mac: %s\n",_IpOrHostnameIsMac ? "true" : "false");
    }

public:

    BaseNetworkInverter():
    _IpOrHostnameIsMac(false),
    _port(0),
    _resolvedIpByMacAdress(nullptr){
        _macToIpResolverTimer.set(TIMER_FAILED_MAC_IP_RESOLUTION);
        _macToIpResolverTimer.zero();
        _macUnknownTimer.set(TIMER_NOT_FOUND_RESET_MAC_IP_RESOLUTION);
        _macUnknownTimer.zero();
    }
    ~BaseNetworkInverter() = default;


    virtual void setHostnameOrIpOrMac(const char * ip){
        _oringalIpOrHostname = String(ip);
        checkIfIpOrHostnameIsMac();
        _resolvedIpByMacAdress = nullptr;
        checkForMacResolution(true);
        hostOrPortUpdated();
    }

    void setPort(uint16_t port){
        _port = port;
        hostOrPortUpdated();
    }

    void setHostnameOrIpOrMacAndPort(const char * ip,uint16_t port){
        _oringalIpOrHostname = String(ip);
        checkIfIpOrHostnameIsMac();
        _resolvedIpByMacAdress = nullptr;
        checkForMacResolution(true);
        _port = port;
        hostOrPortUpdated();
    }

    void setUsernameAndPassword(const char* username, const char* password){
        _username = String(username);
        _password = String(password);
    }

    const String& getUsername() const { return _username; }
    const String& getPassword() const { return _password; }

protected:
    virtual String LogTag() = 0;
    virtual void hostOrPortUpdated(){};

    bool _IpOrHostnameIsMac;
    uint16_t _port;
    std::unique_ptr<std::string> _resolvedIpByMacAdress;
    String _oringalIpOrHostname;
    String _username;
    String _password;

    bool checkForMacResolution(bool force){
        if(_IpOrHostnameIsMac){
            if(force || _macToIpResolverTimer.occured()){
                ESP_LOGD(LogTag().c_str(),"Try to resolve Mac: %s to ip\n",_oringalIpOrHostname.c_str());
                auto apMacsAndIps = InverterUtils::getConnectedClients();
                auto ip = String(_oringalIpOrHostname.c_str());
                ip.toUpperCase();
                auto found = apMacsAndIps->find(std::string(ip.c_str()));
                if(found != apMacsAndIps->end()){
                    ESP_LOGD(LogTag().c_str(),"Found ip for mac is: %s\n",found->second.c_str());
                    if(found->second != "0.0.0.0" && !found->second.empty()){
                        ESP_LOGI(LogTag().c_str(),"Resolved Mac to ip: %s\n", found->second.c_str());
                        if(_resolvedIpByMacAdress == nullptr || found->second != *_resolvedIpByMacAdress){
                            _resolvedIpByMacAdress = std::make_unique<std::string>(found->second);
                            _macToIpResolverTimer.set(TIMER_SUCCESS_MAC_IP_RESOLUTION);
                            _macUnknownTimer.reset();
                            return true;
                        }
                        _macToIpResolverTimer.set(TIMER_FAILED_MAC_IP_RESOLUTION);
                        _macUnknownTimer.reset();
                        ESP_LOGD(LogTag().c_str(),"Resolved Ip is same as before\n");
                        return false;
                    }
                }
                _macToIpResolverTimer.set(TIMER_FAILED_MAC_IP_RESOLUTION);
                ESP_LOGD(LogTag().c_str(),"Failed on resolving Mac to Ip\n");
                return false;
            }
            if(_resolvedIpByMacAdress != nullptr && _macUnknownTimer.occured()){
                _resolvedIpByMacAdress = nullptr;
                ESP_LOGW(LogTag().c_str(),"Removed ip from alreadyResolved by mac address because not seen in a while (15min)\n");
            }
        }
        return false;
    }

    bool checkForMacResolution(){
        return checkForMacResolution(false);
    }
};

using BaseNetworkInverterClass = BaseNetworkInverter<BaseStatistics,BaseDevInfo,BaseAlarmLog,BasePowerCommand>;
