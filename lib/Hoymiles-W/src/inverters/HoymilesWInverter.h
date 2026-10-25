#pragma once

#include <inverter/BaseNetworkInverter.h>
#include "parser/HoymilesWDevInfo.h"
#include <parser/DefaultAlarmLog.h>
#include "parser/HoymilesWStatisticsParser.h"
#include "parser/PowerCommandParser.h"
#include <cstdint>
#include <vector>
#include <TimeoutHelper.h>
#include <AsyncTCP.h>
#include <mutex>
#include <CRC16.h>
#include "pb_decode.h"

#define HOYMILES_W_RECONNECT_DELAY_MS 30000

class HoymilesWInverter : public BaseNetworkInverter<HoymilesWStatisticsParser,HoymilesWDevInfo,DefaultAlarmLog,PowerCommandParser> {
public:
    explicit HoymilesWInverter(uint64_t serial);
    ~HoymilesWInverter();

    struct{
        uint32_t SendRequests;
        uint32_t SuccessfulRequests;
        uint32_t Disconnects;
        uint32_t ConnectionTimeouts;
    } ConnectionStatistics = {};

    uint64_t serial() const override;
    String typeName() const override;
    bool isProducing() override;
    bool isReachable() override;
    bool sendActivePowerControlRequest(float limit, PowerLimitControlType type) override;
    bool resendPowerControlRequest() override;
    bool sendRestartControlRequest() override;
    bool sendPowerControlRequest(bool turnOn) override;
    bool sendDtuRestartRequest() override;

    void update();

    inverter_type getInverterType() const override;
    void setEnableCommands(const bool enabled) override;
    void startConnection();
    void swapBuffers(const InverterData *data);
    bool supportsPowerDistributionLogic() override;
    void onPollTimeChanged() override;
    void resetStats() override;

protected:
    void hostOrPortUpdated() override;
    String LogTag() override;

private:
    enum class ConnState { Offline, Connecting, Connected, Error };

    enum PendingResponse { NONE, REALDATA, HISTPOWER, GETCONFIG, COMMAND, RESTARTDEVICE, RESTARTINVERTER, POWER };

    uint64_t _serial;

    AsyncClient* _client;
    ConnState _connState;
    String _serverIP;
    uint16_t _serverPort;
    uint32_t _lastSeen;

    CRC16 _crc;
    std::vector<uint8_t> _rxBuffer;
    InverterData _inverterData;
    std::mutex _inverterDataMutex;
    PendingResponse _pendingResponse;
    bool _updateReceived;
    bool _updateFailed;
    bool _statisticsInitialized;
    uint64_t _dtuSerial;

    TimeoutHelper _reconnectTimer;
    TimeoutHelper _responseTimer;
    TimeoutHelper _dataUpdateTimer;
    TimeoutHelper _dataStatisticTimer;

    bool _changeCheckedClearOnDisconnect;

    uint64_t getInternalPollTime();
    bool isConnected();

    void connect();
    void disconnect();
    void initializeCRC();

    void onConnect();
    void onDisconnect();
    void onError(int8_t error);
    void onDataReceived(void* data, size_t len);
    void processRxFrames();

    bool requestDataUpdate();
    bool requestStatisticUpdate();
    void setPowerLimit(int limit);
    void requestRestartDevice();
    std::unique_ptr<InverterData> newDataAvailable();
    bool lastRequestFailed();
    bool isSerialValid(const uint64_t serial) const;
    uint64_t getReadSerial() const;
    bool statisticsReceived();

    void printDataAsTextToSerial();

    // Protobuf TX/RX
    void writeReqRealDataNew();
    void readRespRealDataNew(pb_istream_t istream);
    void writeReqAppGetHistPower();
    void readRespAppGetHistPower(pb_istream_t istream);
    void writeReqGetConfig();
    void readRespGetConfig(pb_istream_t istream);
    bool writeReqCommand(uint8_t setPercent);
    bool readRespCommand(pb_istream_t istream);
    bool writeCommandRestartDevice();
    bool readRespCommandRestartDevice(pb_istream_t istream);
    bool writeCommandRestartInverter();
    bool readRespCommandRestartInverter(pb_istream_t istream);
    bool writeCommandPower(bool turnOn);
    bool readRespCommandPower(pb_istream_t istream);
    static bool encodeMiToSn(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

    void requestRestartInverter();
    void requestPowerControl(bool turnOn);

    static float calcValue(int32_t value, int32_t divider = 10);
};
