#pragma once

#include "types.h"
#include "BaseInverter.h"
#include "parser/DeyeStatistics.h"
#include "parser/DeyeDevInfo.h"
#include "parser/DeyeSystemConfigPara.h"
#include "parser/DeyeAlarmLog.h"
#include "parser/DeyeGridProfile.h"
#include "parser/DeyePowerCommand.h"
#include "parser/StatisticsParser.h"
#include "parser/SystemConfigParaParser.h"
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

class DeyeInverter : public BaseInverter<StatisticsParser,DeyeDevInfo,SystemConfigParaParser,DeyeAlarmLog,DeyeGridProfile,DeyePowerCommand> {
public:
    explicit DeyeInverter(uint64_t serial);
    ~DeyeInverter() = default;

    uint64_t serial() override;

    String typeName() override;

    bool isProducing() override;

    bool isReachable() override;

    bool sendActivePowerControlRequest(float limit, PowerLimitControlType type) override;

    bool resendPowerControlRequest() override;

    bool sendRestartControlRequest() override;

    bool sendPowerControlRequest(bool turnOn) override;

    void setHostnameOrIp(const char * hostOrIp);
    void setPort(uint16_t port);

    void updateSocket();

    bool parseInitInformation(size_t length);
    int handleRegisterRead(size_t length);

    void spwapBuffers();

    bool resolveHostname();

    inverter_type getInverterType() override;

private:

    void sendSocketMessage(String message);
    void sendCurrentRegisterRead();
    char _readBuff[1000];

    std::unique_ptr<UDP> _socket;

    std::unique_ptr<IPAddress> _ipAdress;

    //these timers seem to work good no idea what's best and what causes what
    static const uint32_t TIMER_FETCH_DATA = 5 * 60 * 1000;
    static const uint32_t TIMER_HEALTH_CHECK = 20 * 1000;
    static const uint32_t TIMER_ERROR_BACKOFF = 3 * 1000;
    static const uint32_t TIMER_BETWEEN_SENDS = 200;
    static const uint32_t TIMER_RESOLVE_HOSTNAME = 30 * 1000;
    static const uint32_t TIMER_TIMEOUT = 1200;

    static const uint32_t INIT_COMMAND_START_SKIP = 2;

    bool _needInitData;

    uint32_t _timerFullPoll = 0;
    uint32_t _timerHealthCheck = 0;
    uint32_t _timerErrorBackOff = 0;
    uint32_t _timerBetweenSends = 0;
    uint32_t _timerTimeoutCheck = 0;
    uint32_t _timerResolveHostname = 0;
    uint32_t _commandPosition;

    bool _startCommand;
    static const std::vector<RegisterMapping> _registersToRead;

    uint64_t _serial;
    char _hostnameOrIp[MAX_NAME_HOST] = "";
    uint16_t _port = 0;

    uint8_t _payloadStatisticBuffer[STATISTIC_PACKET_SIZE] = {};

    static unsigned int hex_char_to_int(char c);

    static unsigned short modbusCRC16FromHex(const String &message);

    static String modbusCRC16FromASCII(const String &input);

    void appendFragment(uint8_t offset, uint8_t *payload, uint8_t len);
};