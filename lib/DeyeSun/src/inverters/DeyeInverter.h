#pragma once

#include "types.h"
#include <inverter/BaseNetworkInverter.h>
#include "parser/DeyeDevInfo.h"
#include "parser/DeyeAlarmLog.h"
#include <parser/DefaultStatisticsParser.h>
#include "parser/PowerCommandParser.h"
#include <Arduino.h>
#include <cstdint>
#include <list>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <TimeoutHelper.h>

struct RegisterMapping{
    String readRegister;
    uint8_t length;
    uint16_t targetPos;
    RegisterMapping(const String & readRegister,uint8_t length,uint16_t targetPos):
    readRegister(readRegister),
    length(length),
    targetPos(targetPos){}
};

struct WriteRegisterMapping{
    String writeRegister;
    uint8_t length;
    String valueToWrite;

    WriteRegisterMapping(const String &writeRegister, uint8_t length, const String &valueToWrite):
    writeRegister(writeRegister),
    length(length),
    valueToWrite(valueToWrite){}
};

class DeyeInverter : public BaseNetworkInverter<DefaultStatisticsParser,DeyeDevInfo,DeyeAlarmLog,PowerCommandParser> {
public:
    explicit DeyeInverter(uint64_t serial);
    virtual ~DeyeInverter() = default;

    uint64_t serial() const override;

    String typeName() const override;

    bool isProducing() override;

    bool isReachable() override;

    bool sendActivePowerControlRequest(float limit, PowerLimitControlType type) override;

    bool resendPowerControlRequest() override;

    bool sendRestartControlRequest() override;

    bool sendPowerControlRequest(bool turnOn) override;

    void update();

    inverter_type getInverterType() const override;

    static String serialToModel(uint64_t serial);

    void setEnableCommands(const bool enabled) override;

    bool supportsPowerDistributionLogic() override;
private:
    bool parseInitInformation(size_t length);
    int handleRegisterRead(size_t length);
    int handleRegisterWrite(size_t length);
    
    String filterReceivedResponse(size_t length);

    bool resolveHostname();
    void swapBuffers(bool fullData);

    bool handleRead();
    void handleWrite();

    void endSocket();

    void sendSocketMessage(String message);
    void sendCurrentRegisterRead();
    void sendCurrentRegisterWrite();
    char _readBuff[1000];

    std::unique_ptr<UDP> _socket;
    std::unique_ptr<UDP> _oldSocket;

    std::unique_ptr<bool> _powerTargetStatus;
    std::unique_ptr<uint16_t > _limitToSet;

    std::unique_ptr<IPAddress> _ipAdress;

    std::unique_ptr<WriteRegisterMapping> _currentWritCommand;

    //these timers seem to work good no idea what's best and what causes what
    static const uint32_t TIMER_FULL_POLL = 5 * 60 * 1000;
    static const uint32_t TIMER_HEALTH_CHECK = 20 * 1000;
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

    bool _waitLongAfterTimeout;

    bool _startCommand;
    virtual const std::vector<RegisterMapping> & getRegisteresToRead() = 0;
    int _errorCounter;

    uint64_t _serial;

    uint8_t _payloadStatisticBuffer[STATISTIC_PACKET_SIZE] = {};

    static unsigned int hex_char_to_int(char c);

    static unsigned short modbusCRC16FromHex(const String &message);

    static String modbusCRC16FromASCII(const String &input);

    void appendFragment(uint8_t offset, uint8_t *payload, uint8_t len);
};