#pragma once

#include <inverter/BaseNetworkInverter.h>
#include <Arduino.h>
#include <cstdint>
#include <list>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <TimeoutHelper.h>
#include "DeyeInverter.h"

struct RegisterMapping{
    String readRegister;
    uint8_t length;
    uint16_t targetPos;
    RegisterMapping(const String & readRegister,uint8_t length,uint16_t targetPos):
    readRegister(readRegister),
    length(length),
    targetPos(targetPos){}
};

class AtCommandsDeyeInverter : public DeyeInverter{
public:
    explicit AtCommandsDeyeInverter(uint64_t serial);
    virtual ~AtCommandsDeyeInverter() = default;

    bool isReachable() override;

    void update() override;

    deye_inverter_type getDeyeInverterType() const override;

    void setEnableCommands(const bool enabled) override;

    bool supportsPowerDistributionLogic() override;

    struct{
        // TX Request Data
        uint32_t SendCommands;
        uint32_t TimeoutCommands;
        uint32_t ErrorCommands;

        uint32_t Connects;
        uint32_t ConnectsSuccessful;

        uint32_t HealthChecks;
        uint32_t HealthChecksSuccessful;

        uint32_t WriteRequests;
        uint32_t WriteRequestsSuccessful;

        uint32_t ReadRequests;
        uint32_t ReadRequestsSuccessful;
    } ConnectionStatistics = {};

    void resetStats() override;
protected:
    void onPollTimeChanged() override;
    String LogTag() override;
private:
    bool parseInitInformation(size_t length);
    int handleRegisterRead(size_t length);
    int handleRegisterWrite(size_t length);
    
    String filterReceivedResponse(size_t length);

    uint64_t getInternalPollTime();

    bool resolveHostname();
    void swapBuffers(bool fullData);

    bool handleRead();
    void handleWrite();

    void endSocket();

    void sendSocketMessage(const String & message);
    void sendCurrentRegisterRead();
    void sendCurrentRegisterWrite();
    char _readBuff[1000];

    std::unique_ptr<UDP> _socket;
    std::unique_ptr<UDP> _oldSocket;

    std::unique_ptr<IPAddress> _ipAdress;

    //these timers seem to work good no idea what's best and what causes what
    static const uint32_t TIMER_FULL_POLL = 5 * 60 * 1000;
    //static const uint32_t TIMER_HEALTH_CHECK = 20 * 1000;
    static const uint32_t TIMER_ERROR_BACKOFF = 3 * 1000;
    static const uint32_t TIMER_BETWEEN_SENDS = 200;
    static const uint32_t TIMER_RESOLVE_HOSTNAME = 1000 * 30;
    static const uint32_t TIMER_RESOLVE_HOSTNAME_LONG = 60 * 1000 * 5;
    static const uint32_t TIMER_TIMEOUT = 1200;
    static const uint32_t TIMER_COUNTER_ERROR_TIMEOUT = 1 * 60 * 1000;

    //TODO move to inverter classes
    static const uint32_t INIT_COMMAND_START_SKIP = 2;
    static const uint32_t LAST_HEALTHCHECK_COMMEND = 4;

    bool _needInitData;

    TimeoutHelper _timerFullPoll;
    TimeoutHelper _timerHealthCheck;
    TimeoutHelper _timerErrorBackOff;
    uint32_t _timerBetweenSends = 0;
    TimeoutHelper _timerTimeoutCheck;
    TimeoutHelper _timerResolveHostname;
    TimeoutHelper _timerAfterCounterTimout;
    uint32_t _commandPosition;

    bool _startCommand;
    virtual const std::vector<RegisterMapping> & getRegisteresToRead() = 0;
    int _errorCounter;

    uint8_t _payloadStatisticBuffer[STATISTIC_PACKET_SIZE] = {};

    void appendFragment(uint8_t offset, uint8_t *payload, uint8_t len);
};