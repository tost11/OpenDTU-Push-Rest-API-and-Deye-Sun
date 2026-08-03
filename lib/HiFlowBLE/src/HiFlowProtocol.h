#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

/**
 * HiFlow BLE frame encoding/decoding and CRC16-Modbus.
 * 
 * Frame format (shared header for V0 and V1):
 *   [0:2]   "HM" magic (0x484D)
 *   [2:4]   cmd  (big-endian uint16)
 *   [4:6]   tid  (big-endian uint16, monotonic transaction ID)
 *   [6:8]   CRC16-Modbus of ciphertext
 *   [8:10]  length = len(ciphertext) + 10
 *   [10:N]  ciphertext
 *   [N:N+16] GCM tag (V1 only)
 */
namespace HiFlowProtocol {

// Frame header size
static constexpr size_t HEADER_SIZE = 10;
static constexpr size_t GCM_TAG_SIZE = 16;

// Magic bytes
static constexpr uint8_t MAGIC_H = 0x48; // 'H'
static constexpr uint8_t MAGIC_M = 0x4D; // 'M'

// Command codes (big-endian in frame, stored as uint16_t host order here)
static constexpr uint16_t CMD_APP_INFO_DATA_RES_DTO = 0xA301; // V0 pairing request (app→device)
static constexpr uint16_t CMD_APP_INFO_DATA_REQ_DTO = 0xA201; // V0 pairing response (device→app)
static constexpr uint16_t CMD_HB_RES_DTO            = 0xA302; // Heartbeat
static constexpr uint16_t CMD_REAL_DATA_RES_DTO     = 0xA303; // Legacy real data
static constexpr uint16_t CMD_COMMAND_RES_DTO       = 0xA305; // Control commands
static constexpr uint16_t CMD_GET_CONFIG            = 0xA309; // Read config
static constexpr uint16_t CMD_SET_CONFIG            = 0xA310; // Write config
static constexpr uint16_t CMD_REAL_RES_DTO          = 0xA311; // RealDataNew (primary)
static constexpr uint16_t CMD_NETWORK_INFO_RES      = 0xA314; // Network info
static constexpr uint16_t CMD_APP_GET_HIST_POWER_RES = 0xA315;
static constexpr uint16_t CMD_APP_GET_HIST_ED_RES   = 0xA316;
static constexpr uint16_t CMD_COMM_CMD_RES_DTO      = 0xA318; // CommCmd handshake send
static constexpr uint16_t CMD_COMM_CMD_STATUS_RES   = 0xA319; // CommCmd handshake poll

// Response command = request cmd - 0x0100
static constexpr uint16_t CMD_RESPONSE_OFFSET = 0x0100;

// V0 commands (use CBC encryption)
static inline bool isV0Command(uint16_t cmd) {
    return cmd == 0xA201 || cmd == 0xA301 || cmd == 0x8901 || cmd == 0x7901;
}

// CommCmd action codes
static constexpr uint8_t ACTION_LOGIN     = 64;
static constexpr uint8_t ACTION_PIN       = 82;
static constexpr uint8_t ACTION_TIME_SYNC = 104;

/**
 * CRC16-Modbus calculation.
 * Polynomial: 0xA001 (reflected 0x8005)
 * Initial: 0xFFFF, no final XOR
 */
uint16_t crc16Modbus(const uint8_t* data, size_t len);

/**
 * Build a V1 frame (AES-128-GCM encrypted).
 * @param encRand 16-byte device session key
 * @param cmd Command code
 * @param tid Transaction ID
 * @param plaintext Payload to encrypt
 * @param plaintextLen Length
 * @param frameOut Output vector for complete frame
 * @return true on success
 */
bool buildFrameV1(const uint8_t encRand[16], uint16_t cmd, uint16_t tid,
                  const uint8_t* plaintext, size_t plaintextLen,
                  std::vector<uint8_t>& frameOut);

/**
 * Build a V0 frame (AES-128-CBC encrypted).
 * @param sn 12-char serial number
 * @param cmd Command code
 * @param tid Transaction ID
 * @param plaintext Payload
 * @param plaintextLen Length
 * @param frameOut Output vector
 * @return true on success
 */
bool buildFrameV0(const char* sn, uint16_t cmd, uint16_t tid,
                  const uint8_t* plaintext, size_t plaintextLen,
                  std::vector<uint8_t>& frameOut);

/**
 * Parse frame header without decrypting.
 * @param frame Raw frame bytes
 * @param frameLen Length
 * @param cmd Output: command code
 * @param tid Output: transaction ID
 * @param crc Output: CRC from header
 * @param payloadLen Output: length of ciphertext (not including tag)
 * @return true if header is valid
 */
bool parseFrameHeader(const uint8_t* frame, size_t frameLen,
                      uint16_t& cmd, uint16_t& tid, uint16_t& crc, uint16_t& payloadLen);

/**
 * Parse and decrypt a V1 frame.
 * @param encRand 16-byte device key
 * @param frame Complete frame
 * @param frameLen Length
 * @param cmd Output: command code
 * @param tid Output: transaction ID
 * @param plaintext Output: decrypted payload
 * @return true on success (CRC valid, GCM tag valid)
 */
bool parseFrameV1(const uint8_t encRand[16],
                  const uint8_t* frame, size_t frameLen,
                  uint16_t& cmd, uint16_t& tid,
                  std::vector<uint8_t>& plaintext);

/**
 * Parse and decrypt a V0 frame.
 * @param sn 12-char serial
 * @param frame Complete frame
 * @param frameLen Length
 * @param cmd Output: command code
 * @param tid Output: transaction ID
 * @param plaintext Output: decrypted payload
 * @return true on success
 */
bool parseFrameV0(const char* sn,
                  const uint8_t* frame, size_t frameLen,
                  uint16_t& cmd, uint16_t& tid,
                  std::vector<uint8_t>& plaintext);

/**
 * Determine expected total frame size from header.
 * For V0 commands: length field value
 * For V1 commands: length field value + 16 (GCM tag)
 */
size_t expectedFrameSize(const uint8_t* header, size_t headerLen);

// ── Minimal protobuf helpers ──────────────────────────────────────────────

/**
 * Encode a varint into buffer. Returns bytes written.
 */
size_t pbEncodeVarint(uint8_t* buf, uint64_t value);

/**
 * Encode a protobuf varint field (field_number, wire_type=0).
 */
size_t pbEncodeVarintField(uint8_t* buf, uint8_t fieldNum, uint64_t value);

/**
 * Encode a protobuf length-delimited field (field_number, wire_type=2).
 */
size_t pbEncodeBytesField(uint8_t* buf, uint8_t fieldNum, const uint8_t* data, size_t dataLen);

/**
 * Decode a varint from buffer. Returns bytes consumed, 0 on error.
 */
size_t pbDecodeVarint(const uint8_t* buf, size_t bufLen, uint64_t& value);

/**
 * Find a field in a protobuf message by field number.
 * For varint fields (wire type 0): returns value in 'varintOut'
 * For length-delimited fields (wire type 2): returns pointer and length in 'bytesOut'/'bytesLenOut'
 * @return true if field found
 */
bool pbFindVarintField(const uint8_t* msg, size_t msgLen, uint8_t fieldNum, uint64_t& varintOut);
bool pbFindBytesField(const uint8_t* msg, size_t msgLen, uint8_t fieldNum,
                      const uint8_t*& bytesOut, size_t& bytesLenOut);

} // namespace HiFlowProtocol
