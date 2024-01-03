#pragma once

#include "types.h"
#include "BaseInverter.h"
#include "parser/DeyeStatistics.h"
#include "parser/DeyeDevInfo.h"
#include "parser/DeyeSystemConfigPara.h"
#include "parser/DeyeAlarmLog.h"
#include "parser/DeyeGridProfile.h"
#include "parser/StatisticsParser.h"
#include "parser/SystemConfigParaParser.h"
#include "parser/PowerCommandParser.h"
#include <Arduino.h>
#include <cstdint>
#include <list>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define MAX_NAME_HOST 32

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

class DeyeInverter : public BaseInverter<StatisticsParser,DeyeDevInfo,SystemConfigParaParser,DeyeAlarmLog,DeyeGridProfile,PowerCommandParser> {
public:
    explicit DeyeInverter(uint64_t serial,Print & print);
    virtual ~DeyeInverter() = default;

    uint64_t serial() const override;

    String typeName() const override;

    bool isProducing() override;

    bool isReachable() override;

    bool sendActivePowerControlRequest(float limit, PowerLimitControlType type) override;

    bool resendPowerControlRequest() override;

    bool sendRestartControlRequest() override;

    bool sendPowerControlRequest(bool turnOn) override;

    void setHostnameOrIp(const char * hostOrIp);
    void setPort(uint16_t port);

    void updateSocket();

    inverter_type getInverterType() const override;

    static String serialToModel(uint64_t serial);

private:
    bool parseInitInformation(size_t length);
    int handleRegisterRead(size_t length);
    int handleRegisterWrite(size_t length);

    static String lengthToString(uint8_t length,int fill = 4);
    String filterReceivedResponse(size_t length);

    bool resolveHostname();
    void swapBuffers();

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
    static const uint32_t TIMER_FETCH_DATA = 5 * 60 * 1000;
    static const uint32_t TIMER_HEALTH_CHECK = 20 * 1000;
    static const uint32_t TIMER_ERROR_BACKOFF = 3 * 1000;
    static const uint32_t TIMER_BETWEEN_SENDS = 200;
    static const uint32_t TIMER_RESOLVE_HOSTNAME = 30 * 1000;
    static const uint32_t TIMER_TIMEOUT = 1200;
    static const uint32_t TIMER_COUNTER_ERROR_TIMEOUT = 3 * 60 * 1000;

    //TODO move to inverter classes
    static const uint32_t INIT_COMMAND_START_SKIP = 2;
    static const uint32_t LAST_HEALTHCHECK_COMMEND = 4;

    bool _needInitData;

    uint32_t _timerFullPoll = 0;
    uint32_t _timerHealthCheck = 0;
    uint32_t _timerErrorBackOff = 0;
    uint32_t _timerBetweenSends = 0;
    uint32_t _timerTimeoutCheck = 0;
    uint32_t _timerResolveHostname = 0;
    uint32_t _timerAfterCounterTimout = 0;
    uint32_t _commandPosition;

    bool _waitLongAfterTimeout;

    Print & _messageOutput;
    bool _logDebug;

    bool _startCommand;
    virtual const std::vector<RegisterMapping> & getRegisteresToRead() = 0;
    int _errorCounter;

    uint64_t _serial;
    char _hostnameOrIp[MAX_NAME_HOST] = "";
    uint16_t _port = 0;

    uint8_t _payloadStatisticBuffer[STATISTIC_PACKET_SIZE] = {};

    static unsigned int hex_char_to_int(char c);

    static unsigned short modbusCRC16FromHex(const String &message);

    static String modbusCRC16FromASCII(const String &input);

    void appendFragment(uint8_t offset, uint8_t *payload, uint8_t len);

    void println(const char * message,bool debug = false);
    void println(const StringSumHelper & helper,bool debug = false){ println(helper.c_str(),debug);}
    void print(const char * message,bool debug = false);
    void print(const StringSumHelper & helper,bool debug = false){ print(helper.c_str(),debug);}
};