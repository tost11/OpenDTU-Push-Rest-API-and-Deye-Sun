//
// Created by lukas on 31.05.25.
//

#pragma once

#include "DeyeInverter.h"

class CustomModbusDeyeInverter : public DeyeInverter {

    //static const uint16_t SEND_REQUEST_BUFFER_WITHOUT_MODBUS_FRAME_LENGTH = 28;
    static const uint16_t READ_BUFFER_LENGTH = 200;

    static const uint32_t COMMEND_TIMEOUT = 10;

private:
    AsyncClient _client;

    std::unique_ptr<TimeoutHelper> _readTimeout;
    std::unique_ptr<TimeoutHelper> _writeTimeout;

    //TODO implement long timer after some tries
    TimeoutHelper _reconnectTimeout;
    TimeoutHelper _requestDataTimeout;
    TimeoutHelper _statusPrintTimeout;
    TimeoutHelper _pollDataTimout;

    std::string _requestDataCommand;
    char _readBuffer[READ_BUFFER_LENGTH];//TODO check how many characters needed
    size_t _redBytes;
    std::mutex _readDataLock;

    bool _wasConnecting;

    std::string createReqeustDataCommand(const std::string & modbusFrame);

    void onDataReceived(void* data, size_t len);

protected:
    void onPollTimeChanged() override;
    void hostOrPortUpdated() override;
public:
    virtual ~CustomModbusDeyeInverter();

    explicit CustomModbusDeyeInverter(uint64_t serial);

    deye_inverter_type getDeyeInverterType() const override;

    void update() override;

    bool isReachable() override;

    void resetStats() override;

    bool supportsPowerDistributionLogic() override;

    struct{
        uint32_t SendReadDataRequests;
        uint32_t SuccessfulReadDataRequests;

        uint32_t SendWriteDataRequests;
        uint32_t SuccessfulWriteDataRequests;

        uint32_t Connects;
        uint32_t SuccessfulConnects;
    } ConnectionStatistics = {};
};
