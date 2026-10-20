#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>
#include <NimBLEDevice.h>

#include "HiFlowProtocol.h"

/**
 * HiFlow BLE Interface — manages BLE GATT connection to a single
 * Hoymiles HiFlow Pro (HMS-*-WB) inverter.
 * 
 * Handles:
 * - NimBLE client lifecycle (connect/disconnect/reconnect)
 * - V0 pairing (extract encRand)
 * - CommCmd handshake (login + PIN + time sync)
 * - V1 encrypted data requests (RealDataNew)
 * - Frame reassembly from BLE notifications
 */

// Connection states
enum class HiFlowBLEState : uint8_t {
    Idle,              // Not started
    Connecting,        // BLE GATT connection in progress
    Connected,         // GATT connected, not yet paired
    PairingSend,       // V0 pairing frame sent, waiting for response
    PairingSettle,     // Pairing done, settling before handshake
    Handshaking,       // CommCmd handshake in progress
    Ready,             // Paired + handshake done, can request data
    Requesting,        // Waiting for data response
    Error,             // Fatal error (will retry after backoff)
    Disconnected       // Was connected, now disconnected
};

// Parsed RealDataNew result
struct HiFlowRealData {
    // AC side (from SGSMO)
    uint16_t acVoltage = 0;       // V * 10
    uint16_t acFrequency = 0;     // Hz * 100
    uint16_t acPower = 0;         // W * 10
    uint16_t acReactivePower = 0; // var * 10
    uint16_t acCurrent = 0;       // A * 100
    int16_t  acPowerFactor = 0;   // * 1000
    int16_t  temperature = 0;     // C * 10
    int32_t  warningNumber = 0;   // warning/alarm code (0 = none)
    int32_t  linkStatus = 0;      // inverter state indicator

    // DC side (from PvMO, up to 4 ports)
    struct PvPort {
        uint16_t voltage = 0;     // V * 10
        uint16_t current = 0;     // A * 100
        uint16_t power = 0;       // W * 10
        uint32_t energyTotal = 0; // Wh
        uint32_t energyDaily = 0; // Wh
    } pv[4];
    uint8_t pvCount = 0;

    uint64_t inverterSerial = 0; // inverter serial from SGSMO (differs from BLE name serial)
    uint32_t timestamp = 0;
    bool valid = false;
};

class HiFlowBLEInterface {
public:
    HiFlowBLEInterface();
    ~HiFlowBLEInterface();

    /**
     * Configure the interface with serial number and PIN.
     * The BLE MAC is discovered automatically by scanning for "RMI-{sn}".
     * @param sn 12-char serial tail (the part after "RMI-" in the BLE advertisement name)
     * @param pin BLE PIN for first-time pairing (default "123456")
     */
    void setup(const char* sn, const char* pin = "123456");

    /**
     * Set stored encRand (from previous pairing session).
     * If set, V0 pairing step is skipped.
     */
    void setEncRand(const uint8_t encRand[16]);

    /**
     * Get current encRand (valid after successful pairing).
     */
    const uint8_t* getEncRand() const { return _encRand; }
    bool hasEncRand() const { return _hasEncRand; }

    /**
     * Main loop — call frequently from TaskScheduler.
     * Drives the state machine: connect → pair → handshake → request data.
     */
    void loop();

    /**
     * Request a data update. Returns true if request was initiated.
     * Call this when poll timer fires.
     */
    bool requestDataUpdate();

    /**
     * Check if new data is available (call after loop()).
     */
    bool hasNewData() const { return _newDataAvailable; }

    /**
     * Get the latest real data and clear the new-data flag.
     */
    HiFlowRealData getLatestData();

    /**
     * Send a control command (0xA305).
     * @param action action code (HiFlowProtocol::ACTION_*)
     * @param data data string (CommandResDTO field 7, may be empty)
     * @param miSn inverter serial for mi_to_sn (field 9, 0 = omit)
     * @param devKind dev_kind (field 3, 0 = omit; DEV_KIND_DTU for inverter targeted commands)
     * Returns true if the command frame was sent.
     * The interface transitions to Requesting; result via getCommandResult().
     */
    bool sendControlCommand(uint8_t action, const char* data, uint64_t miSn, uint8_t devKind = 0);

    /**
     * Get the result of the last command and clear the result flag.
     * @param success set to true on ACK with err_code 0, false on error/timeout
     * Returns true if a result was pending.
     */
    bool getCommandResult(bool& success);

    /**
     * Request inverter config (0xA309 GetConfig).
     * The interface transitions to Requesting; result via getConfigResult().
     */
    bool requestGetConfig();

    /**
     * Get the result of the last GetConfig request and clear the result flag.
     * @param limitLevel set to power limit in % * 10 (0 = no limit)
     * Returns true if a result was pending.
     */
    bool getConfigResult(int16_t& limitLevel);

    /**
     * Connection state.
     */
    HiFlowBLEState getState() const { return _state; }
    bool isConnected() const { return _state >= HiFlowBLEState::Connected && _state <= HiFlowBLEState::Requesting; }
    bool isReady() const { return _state == HiFlowBLEState::Ready; }

    /**
     * Disconnect and stop.
     */
    void disconnect();

    /**
     * Enable/disable the connection (for polling on/off).
     */
    void setEnabled(bool enabled) { _enabled = enabled; }

private:
    // BLE connection
    NimBLEClient* _client = nullptr;
    NimBLERemoteCharacteristic* _txChar = nullptr; // Write characteristic
    NimBLERemoteCharacteristic* _rxChar = nullptr; // Notify characteristic

    // Configuration
    char _sn[13] = {};
    char _pin[16] = {};
    char _bleId[20] = {};

    // Encryption state
    uint8_t _encRand[16] = {};
    bool _hasEncRand = false;

    // Protocol state
    HiFlowBLEState _state = HiFlowBLEState::Idle;
    uint16_t _tid = 0; // Transaction ID (monotonic)
    bool _enabled = true;

    // Frame reassembly
    std::vector<uint8_t> _rxBuffer;
    size_t _expectedRxSize = 0;
    bool _frameComplete = false;

    // Handshake state
    uint8_t _handshakeStep = 0;
    uint8_t _handshakeFailCount = 0; // consecutive failures; after 2 -> force V0 re-pairing (stale encRand)
    uint8_t _pollCount = 0;

    // Data
    HiFlowRealData _latestData;
    bool _newDataAvailable = false;
    uint8_t _currentPage = 0;
    uint8_t _totalPages = 0;

    // Outbound request tracking (only one request in flight at a time)
    enum class PendingRequest : uint8_t { None, ControlCmd, GetConfig };
    PendingRequest _pendingRequest = PendingRequest::None;

    // Command (power limit) result
    bool _commandResultValid = false;
    bool _commandSuccess = false;

    // GetConfig result
    bool _configResultValid = false;
    bool _configSuccess = false;
    int16_t _configLimitLevel = 0; // % * 10 (0 = no limit)

    // Timing
    uint32_t _lastActionTime = 0;
    uint32_t _reconnectDelay = 2000;
    uint8_t _reconnectAttempts = 0;
    bool _ntpWaitLogged = false; // Log NTP wait message only once
    static constexpr uint8_t MAX_RECONNECT_ATTEMPTS = 5;
    static constexpr uint32_t HANDSHAKE_TIMEOUT = 30000;
    static constexpr uint32_t REQUEST_TIMEOUT = 15000;

    // Internal methods
    uint16_t nextTid();
    bool doConnect();
    void doDisconnect();
    bool doSendPairingRequest();
    // Returns true if the cached encRand should be dropped (V0 re-pairing needed)
    bool handshakeFailed();
    bool processPairingResponse();
    bool doHandshake();
    bool doRequestRealData(uint8_t page);
    void processReceivedFrame();
    bool sendFrame(const std::vector<uint8_t>& frame);

    // Notification callback
    static void onNotify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify);
    static HiFlowBLEInterface* _activeInstance; // For static callback routing

    // Protobuf parsing
    bool parseRealDataNewResponse(const uint8_t* data, size_t len);
    bool extractEncRandFromAppInfo(const uint8_t* data, size_t len);

    // CommCmd protobuf building
    size_t buildCommCmdRequest(uint8_t* buf, uint8_t action, const char* data);
    size_t buildCommCmdStatusPoll(uint8_t* buf, uint8_t action);

    // BleId generation
    void generateBleId();

    // APPInfoData request building
    size_t buildAppInfoRequest(uint8_t* buf);
    // RealDataNew request building
    size_t buildRealDataNewRequest(uint8_t* buf, uint8_t page);

    // Control command building (CommandResDTO)
    size_t buildControlCommand(uint8_t* buf, uint8_t action, const char* data, uint64_t miSn, uint8_t devKind);
    bool parseCommandAck(const uint8_t* data, size_t len);

    // GetConfig request building (GetConfigResDTO) and response parsing
    size_t buildGetConfigRequest(uint8_t* buf);
    bool parseGetConfigResponse(const uint8_t* data, size_t len);
};
