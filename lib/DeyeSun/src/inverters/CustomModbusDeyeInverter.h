//
// Created by lukas on 31.05.25.
//

#pragma once

#include "DeyeInverter.h"

class CustomModbusDeyeInverter : public DeyeInverter {

    static const uint16_t SEND_REQUEST_BUFFER_LENGTH = 36;
    static const uint16_t READ_BUFFER_LENGTH = 150;

private:
    AsyncClient _client;

    //TODO implement long timer after some tries
    TimeoutHelper _reconnectTimeout;
    TimeoutHelper _requestDataTimeout;
    TimeoutHelper _statusPrintTimeout;
    TimeoutHelper _pollDataTimout;

    char _requestDataCommand[SEND_REQUEST_BUFFER_LENGTH];//TODO check how many characters needed
    char _readBuffer[READ_BUFFER_LENGTH];//TODO check how many characters needed
    size_t _redBytes;
    std::mutex _readDataLock;

    void createReqeustDataCommand();

    void onDataReceived(void* data, size_t len);

protected:
    void onPollTimeChanged() override;

public:
    virtual ~CustomModbusDeyeInverter();

    explicit CustomModbusDeyeInverter(uint64_t serial);

    deye_inverter_type getDeyeInverterType() const override;

    void update() override;

    bool isReachable() override;

    //TODO implment
    bool sendActivePowerControlRequest(float limit, const PowerLimitControlType type) override;
    bool resendPowerControlRequest() override;
    bool sendRestartControlRequest() override;
    bool sendPowerControlRequest(const bool turnOn) override;
    void resetStats() override;

    bool supportsPowerDistributionLogic() override;

protected:
    void hostOrPortUpdated() override;
};
