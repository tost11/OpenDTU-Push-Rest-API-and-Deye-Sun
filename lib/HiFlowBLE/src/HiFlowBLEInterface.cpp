#include "HiFlowBLEInterface.h"
#include "HiFlowCrypto.h"
#include <cstring>
#include <ctime>
#include <Arduino.h>
#include <esp_log.h>

#undef TAG
static const char* TAG = "HiFlowBLE";

// GATT UUIDs
static const NimBLEUUID SERVICE_UUID("0000e0ff-3c17-d293-8e48-14fe2e4da212");
static const NimBLEUUID TX_UUID("0000ffe1-0000-1000-8000-00805f9b34fb");
static const NimBLEUUID RX_UUID("0000ffe2-0000-1000-8000-00805f9b34fb");

// Static instance for callback routing
HiFlowBLEInterface* HiFlowBLEInterface::_activeInstance = nullptr;

HiFlowBLEInterface::HiFlowBLEInterface()
{
    generateBleId();
}

HiFlowBLEInterface::~HiFlowBLEInterface()
{
    doDisconnect();
}

void HiFlowBLEInterface::setup(const char* bleMac, const char* sn, const char* pin)
{
    strncpy(_bleMac, bleMac, sizeof(_bleMac) - 1);
    strncpy(_sn, sn, sizeof(_sn) - 1);
    strncpy(_pin, pin, sizeof(_pin) - 1);
    _state = HiFlowBLEState::Idle;
    _reconnectAttempts = 0;
}

void HiFlowBLEInterface::setEncRand(const uint8_t encRand[16])
{
    memcpy(_encRand, encRand, 16);
    _hasEncRand = true;
}

void HiFlowBLEInterface::generateBleId()
{
    // Simplified BleId generation: use millis + random to create a numeric string
    // Mimics the S-Miles BleIdUtil.b() algorithm but simplified for ESP32
    uint32_t seed = millis() ^ esp_random();
    snprintf(_bleId, sizeof(_bleId), "%018lu", (unsigned long)(seed % 1000000000000000000ULL));
    // Ensure we have something valid
    if (_bleId[0] == '0') _bleId[0] = '1';
}

uint16_t HiFlowBLEInterface::nextTid()
{
    return ++_tid;
}

void HiFlowBLEInterface::loop()
{
    if (!_enabled) {
        if (_state != HiFlowBLEState::Idle && _state != HiFlowBLEState::Disconnected) {
            doDisconnect();
        }
        return;
    }

    uint32_t now = millis();

    switch (_state) {
    case HiFlowBLEState::Idle:
        // Don't start BLE connection until NTP time is valid
        if (time(nullptr) < 1700000000) {
            if (!_ntpWaitLogged) {
                ESP_LOGW(TAG, "Waiting for NTP time sync before starting BLE connection...");
                _ntpWaitLogged = true;
            }
            break;
        }
        _ntpWaitLogged = false;
        // Start connection
        if (now - _lastActionTime >= _reconnectDelay) {
            _state = HiFlowBLEState::Connecting;
            _lastActionTime = now;
        }
        break;

    case HiFlowBLEState::Connecting:
        if (doConnect()) {
            _state = HiFlowBLEState::Connected;
            _reconnectAttempts = 0;
            _reconnectDelay = 2000;
            _lastActionTime = now;
            ESP_LOGI(TAG, "BLE connected to %s", _bleMac);
        } else {
            _reconnectAttempts++;
            if (_reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
                ESP_LOGE(TAG, "BLE connect failed after %d attempts", _reconnectAttempts);
                _state = HiFlowBLEState::Error;
                _reconnectDelay = 30000; // Back off 30s
            } else {
                _reconnectDelay = _reconnectDelay * 2; // Exponential backoff
                _state = HiFlowBLEState::Idle;
            }
            _lastActionTime = now;
        }
        break;

    case HiFlowBLEState::Connected:
        // Need to pair (extract encRand) if we don't have it
        if (!_hasEncRand) {
            // Send V0 pairing frame (non-blocking)
            if (doSendPairingRequest()) {
                _state = HiFlowBLEState::PairingSend;
                _lastActionTime = millis();
            } else {
                ESP_LOGE(TAG, "Failed to send V0 pairing request");
                _state = HiFlowBLEState::Error;
                _lastActionTime = millis();
            }
        } else {
            // Reconnect case — encRand already cached, skip V0, reset tid for fresh V1 session
            _tid = 0;
            _state = HiFlowBLEState::PairingSettle;
            _lastActionTime = millis();
        }
        break;

    case HiFlowBLEState::PairingSend:
        // Wait for V0 pairing response (non-blocking)
        if (_frameComplete) {
            if (processPairingResponse()) {
                ESP_LOGI(TAG, "V0 pairing successful, encRand extracted");
                // After V0 pairing, disconnect and reconnect to start fresh V1 session
                // The device sometimes rejects the first V1 frame after V0 pairing on the same connection
                ESP_LOGI(TAG, "Disconnecting after V0 to start fresh V1 session...");
                doDisconnect();
                _state = HiFlowBLEState::Idle;
                _lastActionTime = millis();
                _reconnectDelay = 2000; // Wait 2s before reconnecting
                _reconnectAttempts = 0;
            } else {
                ESP_LOGE(TAG, "V0 pairing response parse failed");
                _state = HiFlowBLEState::Error;
                _lastActionTime = millis();
            }
        } else if (millis() - _lastActionTime > 10000) {
            ESP_LOGE(TAG, "V0 pairing response timeout");
            _state = HiFlowBLEState::Error;
            _lastActionTime = millis();
        }
        break;

    case HiFlowBLEState::PairingSettle:
        // Settle delay before starting handshake (encRand already cached from previous V0 session)
        if (millis() - _lastActionTime >= 500) {
            _state = HiFlowBLEState::Handshaking;
            _handshakeStep = 0;
            _lastActionTime = millis();
            ESP_LOGI(TAG, "Starting CommCmd handshake (time=%ld)", (long)time(nullptr));
        }
        break;

    case HiFlowBLEState::Handshaking:
        // Check if connection was lost during handshake
        if (!_client || !_client->isConnected()) {
            ESP_LOGW(TAG, "Connection lost during handshake, will retry...");
            doDisconnect();
            _state = HiFlowBLEState::Idle;
            _lastActionTime = millis();
            _reconnectDelay = 3000; // Wait 3s before retrying
            break;
        }
        if (doHandshake()) {
            ESP_LOGI(TAG, "CommCmd handshake complete");
            _state = HiFlowBLEState::Ready;
            _lastActionTime = millis();
        } else if (millis() - _lastActionTime > HANDSHAKE_TIMEOUT) {
            ESP_LOGE(TAG, "Handshake timeout");
            _state = HiFlowBLEState::Error;
            _lastActionTime = millis();
        }
        break;

    case HiFlowBLEState::Ready:
        // Check if connection was lost while idle
        if (!_client || !_client->isConnected()) {
            ESP_LOGW(TAG, "BLE connection lost (idle), will reconnect...");
            doDisconnect();
            _state = HiFlowBLEState::Idle;
            _lastActionTime = millis();
            _reconnectDelay = 3000;
        }
        // Idle, waiting for requestDataUpdate() to be called
        break;

    case HiFlowBLEState::Requesting:
        // Check if connection was lost while waiting for response
        if (!_client || !_client->isConnected()) {
            ESP_LOGW(TAG, "BLE connection lost (requesting), will reconnect...");
            doDisconnect();
            _state = HiFlowBLEState::Idle;
            _lastActionTime = millis();
            _reconnectDelay = 3000;
            break;
        }
        // Waiting for response
        if (_frameComplete) {
            processReceivedFrame();
            _frameComplete = false;
        } else if (now - _lastActionTime > REQUEST_TIMEOUT) {
            ESP_LOGW(TAG, "Data request timeout");
            _state = HiFlowBLEState::Ready; // Go back to ready for retry
            _lastActionTime = now;
        }
        break;

    case HiFlowBLEState::Error:
        if (now - _lastActionTime >= _reconnectDelay) {
            doDisconnect();
            _state = HiFlowBLEState::Idle;
            _lastActionTime = now;
        }
        break;

    case HiFlowBLEState::Disconnected:
        if (now - _lastActionTime >= _reconnectDelay) {
            _state = HiFlowBLEState::Idle;
            _lastActionTime = now;
        }
        break;
    }
}

bool HiFlowBLEInterface::requestDataUpdate()
{
    if (_state != HiFlowBLEState::Ready) {
        return false;
    }

    _currentPage = 0;
    _totalPages = 1; // Will be updated from first response
    _newDataAvailable = false;
    _latestData = HiFlowRealData{};

    if (doRequestRealData(0)) {
        _state = HiFlowBLEState::Requesting;
        _lastActionTime = millis();
        return true;
    }
    return false;
}

HiFlowRealData HiFlowBLEInterface::getLatestData()
{
    _newDataAvailable = false;
    return _latestData;
}

void HiFlowBLEInterface::disconnect()
{
    doDisconnect();
    _state = HiFlowBLEState::Idle;
}

// ── BLE Connection ────────────────────────────────────────────────────────────

bool HiFlowBLEInterface::doConnect()
{
    // Initialize NimBLE if not already
    if (!NimBLEDevice::isInitialized()) {
        NimBLEDevice::init("OpenDTU-HiFlow");
        NimBLEDevice::setMTU(512);
    }

    // Scan for the device first to discover the correct address type
    ESP_LOGI(TAG, "Scanning for BLE device %s ...", _bleMac);
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);

    // Scan for up to 15 seconds
    NimBLEScanResults results = scan->getResults(15000, false);

    // Search for our target device by MAC
    std::string targetMacLower(_bleMac);
    for (auto& c : targetMacLower) c = tolower(c);

    const NimBLEAdvertisedDevice* foundDevice = nullptr;
    for (int i = 0; i < results.getCount(); i++) {
        const NimBLEAdvertisedDevice* dev = results.getDevice(i);
        std::string devMac = dev->getAddress().toString();
        if (devMac == targetMacLower) {
            foundDevice = dev;
            ESP_LOGI(TAG, "Found device: %s name=%s type=%d", devMac.c_str(),
                     dev->haveName() ? dev->getName().c_str() : "(none)",
                     dev->getAddress().getType());

            // Extract SN from advertisement name (RMI-XXXXXXXXXXXX)
            if (dev->haveName()) {
                std::string name = dev->getName();
                if (name.rfind("RMI-", 0) == 0 && name.length() >= 16) {
                    std::string sn = name.substr(4, 12);
                    strncpy(_sn, sn.c_str(), sizeof(_sn) - 1);
                    _sn[12] = '\0';
                    ESP_LOGI(TAG, "Extracted SN from BLE name: %s", _sn);
                }
            }
            break;
        }
    }

    if (!foundDevice) {
        ESP_LOGW(TAG, "Device %s not found in scan (%d devices seen)", _bleMac, results.getCount());
        scan->clearResults();
        return false;
    }

    // Create client
    _client = NimBLEDevice::createClient();
    if (!_client) {
        ESP_LOGE(TAG, "Failed to create BLE client");
        scan->clearResults();
        return false;
    }

    _client->setConnectTimeout(10000); // 10 seconds in ms

    // Connect using the discovered device (carries correct address type)
    if (!_client->connect(foundDevice)) {
        ESP_LOGW(TAG, "BLE connect to %s failed", _bleMac);
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
        scan->clearResults();
        return false;
    }
    scan->clearResults();

    // Get service
    NimBLERemoteService* svc = _client->getService(SERVICE_UUID);
    if (!svc) {
        ESP_LOGE(TAG, "HiFlow GATT service not found");
        doDisconnect();
        return false;
    }

    // Get TX characteristic (we write to this)
    _txChar = svc->getCharacteristic(TX_UUID);
    if (!_txChar) {
        ESP_LOGE(TAG, "TX characteristic not found");
        doDisconnect();
        return false;
    }

    // Get RX characteristic (we subscribe to notifications)
    _rxChar = svc->getCharacteristic(RX_UUID);
    if (!_rxChar) {
        ESP_LOGE(TAG, "RX characteristic not found");
        doDisconnect();
        return false;
    }

    // Subscribe to notifications
    _activeInstance = this;
    if (!_rxChar->subscribe(true, onNotify)) {
        ESP_LOGE(TAG, "Failed to subscribe to RX notifications");
        doDisconnect();
        return false;
    }

    return true;
}

void HiFlowBLEInterface::doDisconnect()
{
    if (_client && _client->isConnected()) {
        _client->disconnect();
    }
    if (_client) {
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
    }
    _txChar = nullptr;
    _rxChar = nullptr;
    if (_activeInstance == this) {
        _activeInstance = nullptr;
    }
}

void HiFlowBLEInterface::onNotify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify)
{
    if (!_activeInstance) return;

    auto* self = _activeInstance;

    if (self->_rxBuffer.empty()) {
        // Start of new frame — check for header
        if (length >= HiFlowProtocol::HEADER_SIZE) {
            size_t expected = HiFlowProtocol::expectedFrameSize(pData, length);
            if (expected > 0) {
                self->_expectedRxSize = expected;
                self->_rxBuffer.insert(self->_rxBuffer.end(), pData, pData + length);
            }
        }
    } else {
        // Continuation of existing frame
        self->_rxBuffer.insert(self->_rxBuffer.end(), pData, pData + length);
    }

    // Check if frame is complete
    if (!self->_rxBuffer.empty() && self->_rxBuffer.size() >= self->_expectedRxSize) {
        self->_frameComplete = true;
    }
}

bool HiFlowBLEInterface::sendFrame(const std::vector<uint8_t>& frame)
{
    if (!_txChar || !_client || !_client->isConnected()) {
        return false;
    }

    // Log first bytes of frame for debugging (verbose level)
    ESP_LOGV(TAG, "TX frame (%d bytes): %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X ...",
             (int)frame.size(),
             frame.size() > 0 ? frame[0] : 0, frame.size() > 1 ? frame[1] : 0,
             frame.size() > 2 ? frame[2] : 0, frame.size() > 3 ? frame[3] : 0,
             frame.size() > 4 ? frame[4] : 0, frame.size() > 5 ? frame[5] : 0,
             frame.size() > 6 ? frame[6] : 0, frame.size() > 7 ? frame[7] : 0,
             frame.size() > 8 ? frame[8] : 0, frame.size() > 9 ? frame[9] : 0);

    // Write with response (true) — device requires acknowledged writes for encrypted frames
    return _txChar->writeValue(frame.data(), frame.size(), true);
}

// ── V0 Pairing ───────────────────────────────────────────────────────────────

size_t HiFlowBLEInterface::buildAppInfoRequest(uint8_t* buf)
{
    // Build APPInfoDataResDTO protobuf:
    // field 1 (bytes): time_ymd_hms "YYYY-MM-DD HH:MM:SS"
    // field 4 (varint): offset (timezone offset in seconds)
    // field 5 (varint): time (unix timestamp)

    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

    size_t pos = 0;
    pos += HiFlowProtocol::pbEncodeBytesField(buf + pos, 1, (const uint8_t*)timeStr, strlen(timeStr));
    pos += HiFlowProtocol::pbEncodeVarintField(buf + pos, 4, 3600); // offset (field 4, not 2)
    pos += HiFlowProtocol::pbEncodeVarintField(buf + pos, 5, (uint64_t)now); // time
    return pos;
}

bool HiFlowBLEInterface::doSendPairingRequest()
{
    // Build and send V0 APPInfoData request (non-blocking)
    uint8_t reqBuf[128];
    size_t reqLen = buildAppInfoRequest(reqBuf);

    uint16_t tid = nextTid();
    std::vector<uint8_t> frame;

    if (!HiFlowProtocol::buildFrameV0(_sn, HiFlowProtocol::CMD_APP_INFO_DATA_RES_DTO, tid, reqBuf, reqLen, frame)) {
        ESP_LOGE(TAG, "Failed to build V0 pairing frame");
        return false;
    }

    _rxBuffer.clear();
    _frameComplete = false;
    _expectedRxSize = 0;

    if (!sendFrame(frame)) {
        ESP_LOGE(TAG, "Failed to send V0 pairing frame");
        return false;
    }

    ESP_LOGI(TAG, "V0 pairing request sent, waiting for response...");
    return true;
}

bool HiFlowBLEInterface::processPairingResponse()
{
    // Parse V0 response (called when _frameComplete is true)
    uint16_t respCmd, respTid;
    std::vector<uint8_t> plaintext;

    if (!HiFlowProtocol::parseFrameV0(_sn, _rxBuffer.data(), _rxBuffer.size(), respCmd, respTid, plaintext)) {
        ESP_LOGE(TAG, "Failed to decrypt V0 response");
        _rxBuffer.clear();
        _frameComplete = false;
        return false;
    }

    _rxBuffer.clear();
    _frameComplete = false;

    // Extract encRand from APPInfoDataReqDTO
    return extractEncRandFromAppInfo(plaintext.data(), plaintext.size());
}

bool HiFlowBLEInterface::extractEncRandFromAppInfo(const uint8_t* data, size_t len)
{
    // APPInfoDataReqDTO → field 8 (APPDtuInfoMO, wire type 2 = sub-message)
    const uint8_t* dtuInfo = nullptr;
    size_t dtuInfoLen = 0;

    if (!HiFlowProtocol::pbFindBytesField(data, len, 8, dtuInfo, dtuInfoLen)) {
        ESP_LOGE(TAG, "Field 8 (dtu_info) not found in APPInfoData");
        return false;
    }

    // APPDtuInfoMO → field 27 (enc_rand, bytes)
    const uint8_t* encRandPtr = nullptr;
    size_t encRandLen = 0;

    if (!HiFlowProtocol::pbFindBytesField(dtuInfo, dtuInfoLen, 27, encRandPtr, encRandLen)) {
        ESP_LOGE(TAG, "Field 27 (enc_rand) not found in APPDtuInfoMO");
        return false;
    }

    if (encRandLen != 16) {
        ESP_LOGE(TAG, "enc_rand unexpected length: %d", (int)encRandLen);
        return false;
    }

    memcpy(_encRand, encRandPtr, 16);
    _hasEncRand = true;

    ESP_LOGI(TAG, "encRand extracted successfully");
    return true;
}

// ── CommCmd Handshake ─────────────────────────────────────────────────────────

size_t HiFlowBLEInterface::buildCommCmdRequest(uint8_t* buf, uint8_t action, const char* data)
{
    // CommCmdResDTO:
    // field 1 (varint) = unix timestamp
    // field 2 (varint) = action
    // field 5 (varint) = unix timestamp (tid)
    // field 6 (bytes)  = data string
    time_t now = time(nullptr);
    size_t pos = 0;
    pos += HiFlowProtocol::pbEncodeVarintField(buf + pos, 1, (uint64_t)now);
    pos += HiFlowProtocol::pbEncodeVarintField(buf + pos, 2, action);
    pos += HiFlowProtocol::pbEncodeVarintField(buf + pos, 5, (uint64_t)now);
    if (data && strlen(data) > 0) {
        pos += HiFlowProtocol::pbEncodeBytesField(buf + pos, 6, (const uint8_t*)data, strlen(data));
    }
    return pos;
}

size_t HiFlowBLEInterface::buildCommCmdStatusPoll(uint8_t* buf, uint8_t action)
{
    // CommCmdStatusResDTO:
    // field 1 (varint) = unix timestamp
    // field 2 (varint) = action
    // field 4 (varint) = unix timestamp (tid)
    time_t now = time(nullptr);
    size_t pos = 0;
    pos += HiFlowProtocol::pbEncodeVarintField(buf + pos, 1, (uint64_t)now);
    pos += HiFlowProtocol::pbEncodeVarintField(buf + pos, 2, action);
    pos += HiFlowProtocol::pbEncodeVarintField(buf + pos, 4, (uint64_t)now);
    return pos;
}

bool HiFlowBLEInterface::doHandshake()
{
    // Fully non-blocking handshake state machine.
    // Each step: send a frame, advance to wait state, return false.
    // Wait state: check _frameComplete, parse response, advance to next step.

    // Connection check — if device disconnected mid-handshake, bail out
    if (!_client || !_client->isConnected()) {
        ESP_LOGW(TAG, "BLE disconnected during handshake");
        return false;
    }

    // Step 0: Send login command (action 64)
    if (_handshakeStep == 0) {
        ESP_LOGI(TAG, "Handshake: sending login with bleId");

        // Log encRand for debugging
        ESP_LOGV(TAG, "encRand: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                 _encRand[0], _encRand[1], _encRand[2], _encRand[3],
                 _encRand[4], _encRand[5], _encRand[6], _encRand[7],
                 _encRand[8], _encRand[9], _encRand[10], _encRand[11],
                 _encRand[12], _encRand[13], _encRand[14], _encRand[15]);

        uint8_t reqBuf[128];
        size_t reqLen = buildCommCmdRequest(reqBuf, HiFlowProtocol::ACTION_LOGIN, _bleId);

        ESP_LOGV(TAG, "bleId='%s', reqLen=%d", _bleId, (int)reqLen);

        uint16_t tid = nextTid();
        ESP_LOGV(TAG, "V1 cmd=0x%04X tid=%d", HiFlowProtocol::CMD_COMM_CMD_RES_DTO, tid);

        // Log derived key and nonce
        uint8_t dbgKey[16], dbgNonce[12], dbgAad[4];
        HiFlowCrypto::deriveV1Key(_encRand, dbgKey);
        HiFlowCrypto::deriveV1Nonce(_encRand, HiFlowProtocol::CMD_COMM_CMD_RES_DTO, tid, dbgNonce);
        HiFlowCrypto::buildV1Aad(HiFlowProtocol::CMD_COMM_CMD_RES_DTO, tid, dbgAad);
        ESP_LOGV(TAG, "V1 key: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                 dbgKey[0], dbgKey[1], dbgKey[2], dbgKey[3],
                 dbgKey[4], dbgKey[5], dbgKey[6], dbgKey[7],
                 dbgKey[8], dbgKey[9], dbgKey[10], dbgKey[11],
                 dbgKey[12], dbgKey[13], dbgKey[14], dbgKey[15]);
        ESP_LOGV(TAG, "V1 nonce: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                 dbgNonce[0], dbgNonce[1], dbgNonce[2], dbgNonce[3],
                 dbgNonce[4], dbgNonce[5], dbgNonce[6], dbgNonce[7],
                 dbgNonce[8], dbgNonce[9], dbgNonce[10], dbgNonce[11]);
        ESP_LOGV(TAG, "V1 aad: %02x%02x%02x%02x", dbgAad[0], dbgAad[1], dbgAad[2], dbgAad[3]);

        std::vector<uint8_t> frame;
        if (!HiFlowProtocol::buildFrameV1(_encRand, HiFlowProtocol::CMD_COMM_CMD_RES_DTO, tid, reqBuf, reqLen, frame)) {
            return false;
        }

        _rxBuffer.clear();
        _frameComplete = false;
        _expectedRxSize = 0;

        if (!sendFrame(frame)) return false;

        _handshakeStep = 1;
        _pollCount = 0;
        _lastActionTime = millis();
        return false;
    }

    // Step 1: Wait for login ack, discard it, then send status poll
    if (_handshakeStep == 1) {
        if (millis() - _lastActionTime < 1000) return false; // Wait 1s

        // Discard any pending notification (ack from login command)
        _rxBuffer.clear();
        _frameComplete = false;
        _expectedRxSize = 0;

        // Send status poll
        _lastActionTime = millis();
        uint8_t reqBuf[64];
        size_t reqLen = buildCommCmdStatusPoll(reqBuf, HiFlowProtocol::ACTION_LOGIN);

        uint16_t tid = nextTid();
        std::vector<uint8_t> frame;
        if (!HiFlowProtocol::buildFrameV1(_encRand, HiFlowProtocol::CMD_COMM_CMD_STATUS_RES, tid, reqBuf, reqLen, frame)) {
            return false;
        }

        if (!sendFrame(frame)) return false;

        _handshakeStep = 10; // Wait for poll response
        return false;
    }

    // Step 10: Wait for login poll response
    if (_handshakeStep == 10) {
        if (!_frameComplete) {
            if (millis() - _lastActionTime > 5000) {
                _pollCount++;
                if (_pollCount >= 5) {
                    ESP_LOGE(TAG, "Login poll timeout after 5 attempts");
                    return false;
                }
                _handshakeStep = 1; // Retry poll
            }
            return false;
        }

        // Parse response
        uint16_t respCmd, respTid;
        std::vector<uint8_t> plaintext;
        if (!HiFlowProtocol::parseFrameV1(_encRand, _rxBuffer.data(), _rxBuffer.size(), respCmd, respTid, plaintext)) {
            ESP_LOGW(TAG, "Login poll: decrypt failed");
            _rxBuffer.clear();
            _frameComplete = false;
            _pollCount++;
            _handshakeStep = 1;
            _lastActionTime = millis();
            return false;
        }
        _rxBuffer.clear();
        _frameComplete = false;

        // Debug: log response
        ESP_LOGV(TAG, "Login poll resp cmd=0x%04X, pt_len=%d, pt[0..5]=%02X %02X %02X %02X %02X %02X",
                 respCmd, (int)plaintext.size(),
                 plaintext.size() > 0 ? plaintext[0] : 0,
                 plaintext.size() > 1 ? plaintext[1] : 0,
                 plaintext.size() > 2 ? plaintext[2] : 0,
                 plaintext.size() > 3 ? plaintext[3] : 0,
                 plaintext.size() > 4 ? plaintext[4] : 0,
                 plaintext.size() > 5 ? plaintext[5] : 0);

        uint64_t sts = 0;
        uint64_t action = 0;
        HiFlowProtocol::pbFindVarintField(plaintext.data(), plaintext.size(), 3, action);
        HiFlowProtocol::pbFindVarintField(plaintext.data(), plaintext.size(), 11, sts);
        ESP_LOGV(TAG, "Login poll: action=%d, sts=%d", (int)action, (int)sts);

        if (sts == 1) {
            ESP_LOGI(TAG, "BleId already whitelisted");
            _handshakeStep = 4;
            _lastActionTime = millis();
            return false;
        } else if (sts == 3) {
            ESP_LOGI(TAG, "BleId unknown, PIN required");
            _handshakeStep = 2;
            _lastActionTime = millis();
            return false;
        } else if (sts == 0) {
            _pollCount++;
            _handshakeStep = 1; // Poll again
            _lastActionTime = millis();
            return false;
        }
        _pollCount++;
        _handshakeStep = 1;
        _lastActionTime = millis();
        return false;
    }

    // Step 2: Send PIN (action 82)
    if (_handshakeStep == 2) {
        if (millis() - _lastActionTime < 500) return false;
        ESP_LOGI(TAG, "Handshake: sending PIN");
        uint8_t reqBuf[128];
        size_t reqLen = buildCommCmdRequest(reqBuf, HiFlowProtocol::ACTION_PIN, _pin);

        uint16_t tid = nextTid();
        std::vector<uint8_t> frame;
        if (!HiFlowProtocol::buildFrameV1(_encRand, HiFlowProtocol::CMD_COMM_CMD_RES_DTO, tid, reqBuf, reqLen, frame)) {
            return false;
        }

        _rxBuffer.clear();
        _frameComplete = false;
        _expectedRxSize = 0;

        if (!sendFrame(frame)) return false;

        _handshakeStep = 20; // Wait for ack, then poll
        _pollCount = 0;
        _lastActionTime = millis();
        return false;
    }

    // Step 20: Wait for PIN command ack, discard it, then send status poll
    if (_handshakeStep == 20) {
        if (millis() - _lastActionTime < 1000) return false;

        // Discard any pending notification (ack from PIN command)
        _rxBuffer.clear();
        _frameComplete = false;
        _expectedRxSize = 0;

        // Send status poll
        _lastActionTime = millis();
        uint8_t reqBuf[64];
        size_t reqLen = buildCommCmdStatusPoll(reqBuf, HiFlowProtocol::ACTION_PIN);

        uint16_t tid = nextTid();
        std::vector<uint8_t> frame;
        if (!HiFlowProtocol::buildFrameV1(_encRand, HiFlowProtocol::CMD_COMM_CMD_STATUS_RES, tid, reqBuf, reqLen, frame)) {
            return false;
        }

        if (!sendFrame(frame)) return false;

        _handshakeStep = 30; // Wait for poll response
        return false;
    }

    // Step 30: Wait for PIN poll response
    if (_handshakeStep == 30) {
        if (!_frameComplete) {
            if (millis() - _lastActionTime > 5000) {
                _pollCount++;
                if (_pollCount >= 8) {
                    ESP_LOGE(TAG, "PIN poll timeout");
                    return false;
                }
                _handshakeStep = 20; // Retry: discard + poll again
            }
            return false;
        }

        uint16_t respCmd, respTid;
        std::vector<uint8_t> plaintext;
        if (!HiFlowProtocol::parseFrameV1(_encRand, _rxBuffer.data(), _rxBuffer.size(), respCmd, respTid, plaintext)) {
            ESP_LOGW(TAG, "PIN poll: decrypt failed");
            _rxBuffer.clear();
            _frameComplete = false;
            _pollCount++;
            _handshakeStep = 20;
            _lastActionTime = millis();
            return false;
        }
        _rxBuffer.clear();
        _frameComplete = false;

        // Debug: log response cmd and first bytes of plaintext
        ESP_LOGV(TAG, "PIN poll resp cmd=0x%04X, pt_len=%d, pt[0..5]=%02X %02X %02X %02X %02X %02X",
                 respCmd, (int)plaintext.size(),
                 plaintext.size() > 0 ? plaintext[0] : 0,
                 plaintext.size() > 1 ? plaintext[1] : 0,
                 plaintext.size() > 2 ? plaintext[2] : 0,
                 plaintext.size() > 3 ? plaintext[3] : 0,
                 plaintext.size() > 4 ? plaintext[4] : 0,
                 plaintext.size() > 5 ? plaintext[5] : 0);

        uint64_t sts = 0xFF;
        uint64_t action = 0xFF;
        HiFlowProtocol::pbFindVarintField(plaintext.data(), plaintext.size(), 3, action);
        HiFlowProtocol::pbFindVarintField(plaintext.data(), plaintext.size(), 11, sts);
        ESP_LOGV(TAG, "PIN poll: action=%d, sts=%d", (int)action, (int)sts);

        if (sts == 0 || sts == 0xFF) {
            // sts=0 means PIN accepted; sts=0xFF means field 11 not found in response
            // (the device sometimes omits this field — treat as success, matching thesolarapp behavior)
            if (sts == 0xFF) {
                ESP_LOGW(TAG, "No sts field in PIN response, assuming success");
            }
            ESP_LOGI(TAG, "PIN accepted, bleId whitelisted");
            _handshakeStep = 4;
            _lastActionTime = millis();
            return false;
        } else if (sts == 1) {
            ESP_LOGE(TAG, "Wrong PIN!");
            return false;
        }
        _pollCount++;
        _handshakeStep = 20; // Retry
        _lastActionTime = millis();
        return false;
    }

    // Step 4: Send time sync (action 104)
    if (_handshakeStep == 4) {
        if (millis() - _lastActionTime < 500) return false;
        ESP_LOGI(TAG, "Handshake: sending time sync");
        time_t now = time(nullptr);
        char timeSyncData[32];
        snprintf(timeSyncData, sizeof(timeSyncData), "%ld,3600\r", (long)now);

        uint8_t reqBuf[128];
        size_t reqLen = buildCommCmdRequest(reqBuf, HiFlowProtocol::ACTION_TIME_SYNC, timeSyncData);

        uint16_t tid = nextTid();
        std::vector<uint8_t> frame;
        if (!HiFlowProtocol::buildFrameV1(_encRand, HiFlowProtocol::CMD_COMM_CMD_RES_DTO, tid, reqBuf, reqLen, frame)) {
            return false;
        }

        _rxBuffer.clear();
        _frameComplete = false;
        _expectedRxSize = 0;

        if (!sendFrame(frame)) return false;

        _handshakeStep = 5;
        _pollCount = 0;
        _lastActionTime = millis();
        return false;
    }

    // Step 5: Poll time sync status
    if (_handshakeStep == 5) {
        if (millis() - _lastActionTime < 1000) return false;

        // Check if response arrived
        if (_frameComplete) {
            _rxBuffer.clear();
            _frameComplete = false;
            ESP_LOGI(TAG, "Handshake complete (time synced)");
            return true;
        }

        // Send status poll
        _lastActionTime = millis();
        uint8_t reqBuf[64];
        size_t reqLen = buildCommCmdStatusPoll(reqBuf, HiFlowProtocol::ACTION_TIME_SYNC);

        uint16_t tid = nextTid();
        std::vector<uint8_t> frame;
        if (!HiFlowProtocol::buildFrameV1(_encRand, HiFlowProtocol::CMD_COMM_CMD_STATUS_RES, tid, reqBuf, reqLen, frame)) {
            return false;
        }

        _rxBuffer.clear();
        _frameComplete = false;
        _expectedRxSize = 0;

        if (!sendFrame(frame)) return false;

        _handshakeStep = 50; // Wait for time sync poll response
        return false;
    }

    // Step 50: Wait for time sync poll response
    if (_handshakeStep == 50) {
        if (!_frameComplete) {
            if (millis() - _lastActionTime > 5000) {
                _pollCount++;
                if (_pollCount >= 3) {
                    ESP_LOGW(TAG, "Time sync poll failed, proceeding anyway");
                    return true; // Handshake done regardless
                }
                _handshakeStep = 5;
            }
            return false;
        }

        _rxBuffer.clear();
        _frameComplete = false;
        ESP_LOGI(TAG, "Handshake complete (time synced)");
        return true;
    }

    return false;
}

// ── Data Request ──────────────────────────────────────────────────────────────

size_t HiFlowBLEInterface::buildRealDataNewRequest(uint8_t* buf, uint8_t page)
{
    // RealDataNewResDTO:
    // field 1 (bytes): time_ymd_hms "YYYY-MM-DD HH:MM:SS"
    // field 2 (varint): cp (page number)
    // field 4 (varint): offset (3600)
    // field 5 (varint): time (unix timestamp)

    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

    size_t pos = 0;
    pos += HiFlowProtocol::pbEncodeBytesField(buf + pos, 1, (const uint8_t*)timeStr, strlen(timeStr));
    pos += HiFlowProtocol::pbEncodeVarintField(buf + pos, 2, page);
    pos += HiFlowProtocol::pbEncodeVarintField(buf + pos, 4, 3600);
    pos += HiFlowProtocol::pbEncodeVarintField(buf + pos, 5, (uint64_t)now);
    return pos;
}

bool HiFlowBLEInterface::doRequestRealData(uint8_t page)
{
    uint8_t reqBuf[128];
    size_t reqLen = buildRealDataNewRequest(reqBuf, page);

    uint16_t tid = nextTid();
    std::vector<uint8_t> frame;
    if (!HiFlowProtocol::buildFrameV1(_encRand, HiFlowProtocol::CMD_REAL_RES_DTO, tid, reqBuf, reqLen, frame)) {
        return false;
    }

    _rxBuffer.clear();
    _frameComplete = false;
    _expectedRxSize = 0;

    return sendFrame(frame);
}

void HiFlowBLEInterface::processReceivedFrame()
{
    uint16_t respCmd, respTid;
    std::vector<uint8_t> plaintext;

    if (!HiFlowProtocol::parseFrameV1(_encRand, _rxBuffer.data(), _rxBuffer.size(), respCmd, respTid, plaintext)) {
        ESP_LOGW(TAG, "Failed to decrypt data response (encRand stale?)");
        _rxBuffer.clear();
        _state = HiFlowBLEState::Error;
        _hasEncRand = false; // Force re-pairing
        return;
    }

    _rxBuffer.clear();

    // Expected response cmd = request cmd - 0x0100
    uint16_t expectedCmd = HiFlowProtocol::CMD_REAL_RES_DTO - HiFlowProtocol::CMD_RESPONSE_OFFSET;
    if (respCmd != expectedCmd) {
        ESP_LOGW(TAG, "Unexpected response cmd: 0x%04X (expected 0x%04X)", respCmd, expectedCmd);
        _state = HiFlowBLEState::Ready;
        return;
    }

    // Parse RealDataNew response
    if (parseRealDataNewResponse(plaintext.data(), plaintext.size())) {
        // Check if we need more pages
        if (_currentPage + 1 < _totalPages) {
            _currentPage++;
            if (doRequestRealData(_currentPage)) {
                _lastActionTime = millis();
                // Stay in Requesting state
            } else {
                _state = HiFlowBLEState::Ready;
            }
        } else {
            // All pages received
            _latestData.valid = true;
            _newDataAvailable = true;
            _state = HiFlowBLEState::Ready;
            ESP_LOGI(TAG, "RealDataNew received: AC=%dW, PV ports=%d",
                     _latestData.acPower / 10, _latestData.pvCount);
        }
    } else {
        ESP_LOGW(TAG, "Failed to parse RealDataNew");
        _state = HiFlowBLEState::Ready;
    }
}

bool HiFlowBLEInterface::parseRealDataNewResponse(const uint8_t* data, size_t len)
{
    // RealDataNewReqDTO fields:
    // field 3 (varint): ap = total pages
    // field 4 (varint): cp = current page
    // field 9 (repeated bytes): sgs_data (SGSMO sub-messages)
    // field 11 (repeated bytes): pv_data (PvMO sub-messages)

    // Get page info
    uint64_t ap = 1, cp = 0;
    HiFlowProtocol::pbFindVarintField(data, len, 3, ap);
    HiFlowProtocol::pbFindVarintField(data, len, 4, cp);
    _totalPages = (uint8_t)ap;

    // Parse all SGSMO entries (field 9, repeated)
    // We need to iterate through all field 9 entries
    size_t pos = 0;
    while (pos < len) {
        uint64_t tag;
        size_t tagLen = HiFlowProtocol::pbDecodeVarint(data + pos, len - pos, tag);
        if (tagLen == 0) break;
        pos += tagLen;

        uint8_t fieldNum = (uint8_t)(tag >> 3);
        uint8_t wireType = (uint8_t)(tag & 0x07);

        if (wireType == 2) {
            // Length-delimited field
            uint64_t fieldLen;
            size_t lenBytes = HiFlowProtocol::pbDecodeVarint(data + pos, len - pos, fieldLen);
            if (lenBytes == 0) break;
            pos += lenBytes;

            if (fieldNum == 9 && (pos + fieldLen) <= len) {
                // SGSMO sub-message
                const uint8_t* sgs = data + pos;
                size_t sgsLen = (size_t)fieldLen;

                uint64_t val;
                if (HiFlowProtocol::pbFindVarintField(sgs, sgsLen, 3, val))
                    _latestData.acVoltage = (uint16_t)val;
                if (HiFlowProtocol::pbFindVarintField(sgs, sgsLen, 4, val))
                    _latestData.acFrequency = (uint16_t)val;
                if (HiFlowProtocol::pbFindVarintField(sgs, sgsLen, 5, val))
                    _latestData.acPower = (uint16_t)val;
                if (HiFlowProtocol::pbFindVarintField(sgs, sgsLen, 6, val))
                    _latestData.acReactivePower = (uint16_t)val;
                if (HiFlowProtocol::pbFindVarintField(sgs, sgsLen, 7, val))
                    _latestData.acCurrent = (uint16_t)val;
                if (HiFlowProtocol::pbFindVarintField(sgs, sgsLen, 8, val))
                    _latestData.acPowerFactor = (int16_t)val;
                if (HiFlowProtocol::pbFindVarintField(sgs, sgsLen, 9, val))
                    _latestData.temperature = (int16_t)val;
            } else if (fieldNum == 11 && (pos + fieldLen) <= len) {
                // PvMO sub-message
                const uint8_t* pv = data + pos;
                size_t pvLen = (size_t)fieldLen;

                uint8_t pvIdx = _latestData.pvCount;
                if (pvIdx < 4) {
                    uint64_t val;
                    if (HiFlowProtocol::pbFindVarintField(pv, pvLen, 3, val))
                        _latestData.pv[pvIdx].voltage = (uint16_t)val;
                    if (HiFlowProtocol::pbFindVarintField(pv, pvLen, 4, val))
                        _latestData.pv[pvIdx].current = (uint16_t)val;
                    if (HiFlowProtocol::pbFindVarintField(pv, pvLen, 5, val))
                        _latestData.pv[pvIdx].power = (uint16_t)val;
                    if (HiFlowProtocol::pbFindVarintField(pv, pvLen, 6, val))
                        _latestData.pv[pvIdx].energyTotal = (uint32_t)val;
                    if (HiFlowProtocol::pbFindVarintField(pv, pvLen, 7, val))
                        _latestData.pv[pvIdx].energyDaily = (uint32_t)val;
                    _latestData.pvCount++;
                }
            }

            pos += (size_t)fieldLen;
        } else if (wireType == 0) {
            // Varint
            uint64_t dummy;
            size_t vLen = HiFlowProtocol::pbDecodeVarint(data + pos, len - pos, dummy);
            if (vLen == 0) break;
            pos += vLen;
        } else if (wireType == 5) {
            pos += 4;
        } else if (wireType == 1) {
            pos += 8;
        } else {
            break;
        }
    }

    return _latestData.acPower > 0 || _latestData.pvCount > 0;
}
