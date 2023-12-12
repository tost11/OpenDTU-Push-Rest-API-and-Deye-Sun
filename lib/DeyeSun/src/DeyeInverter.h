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

class DeyeInverter : public BaseInverter<StatisticsParser,DeyeDevInfo,DeyeSystemConfigPara,DeyeAlarmLog,DeyeGridProfile,DeyePowerCommand> {
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
private:

    void sendSocketMessage(String message);
    void sendCurrentRegisterRead();
    char _readBuff[1000];

    std::unique_ptr<UDP> _socket;

    uint32_t _lastPoll = 0;
    uint32_t _lastSuccessfullPoll = 0;
    uint32_t _lastSuccessData = 0;
    uint32_t lastTimeSuccesfullData = 0;
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