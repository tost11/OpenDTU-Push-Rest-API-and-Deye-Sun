#include "HiFlowProtocol.h"
#include "HiFlowCrypto.h"
#include <cstring>

namespace HiFlowProtocol {

uint16_t crc16Modbus(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static void writeHeader(uint8_t* buf, uint16_t cmd, uint16_t tid, uint16_t crc, uint16_t length)
{
    buf[0] = MAGIC_H;
    buf[1] = MAGIC_M;
    buf[2] = (cmd >> 8) & 0xFF;
    buf[3] = cmd & 0xFF;
    buf[4] = (tid >> 8) & 0xFF;
    buf[5] = tid & 0xFF;
    buf[6] = (crc >> 8) & 0xFF;
    buf[7] = crc & 0xFF;
    buf[8] = (length >> 8) & 0xFF;
    buf[9] = length & 0xFF;
}

bool buildFrameV1(const uint8_t encRand[16], uint16_t cmd, uint16_t tid,
                  const uint8_t* plaintext, size_t plaintextLen,
                  std::vector<uint8_t>& frameOut)
{
    // Encrypt
    std::vector<uint8_t> ct(plaintextLen);
    uint8_t tag[16];

    if (!HiFlowCrypto::encryptV1(encRand, cmd, tid, plaintext, plaintextLen, ct.data(), tag)) {
        return false;
    }

    // CRC over ciphertext only
    uint16_t crc = crc16Modbus(ct.data(), ct.size());
    uint16_t length = static_cast<uint16_t>(ct.size() + HEADER_SIZE);

    // Assemble: header + ciphertext + tag
    frameOut.resize(HEADER_SIZE + ct.size() + GCM_TAG_SIZE);
    writeHeader(frameOut.data(), cmd, tid, crc, length);
    memcpy(frameOut.data() + HEADER_SIZE, ct.data(), ct.size());
    memcpy(frameOut.data() + HEADER_SIZE + ct.size(), tag, GCM_TAG_SIZE);

    return true;
}

bool buildFrameV0(const char* sn, uint16_t cmd, uint16_t tid,
                  const uint8_t* plaintext, size_t plaintextLen,
                  std::vector<uint8_t>& frameOut)
{
    // Max ciphertext size: plaintext + up to 16 bytes padding
    size_t maxCtLen = plaintextLen + 16;
    std::vector<uint8_t> ct(maxCtLen);
    size_t ctLen = 0;

    if (!HiFlowCrypto::encryptV0(sn, cmd, tid, plaintext, plaintextLen, ct.data(), ctLen)) {
        return false;
    }

    uint16_t crc = crc16Modbus(ct.data(), ctLen);
    uint16_t length = static_cast<uint16_t>(ctLen + HEADER_SIZE);

    // V0: no tag appended
    frameOut.resize(HEADER_SIZE + ctLen);
    writeHeader(frameOut.data(), cmd, tid, crc, length);
    memcpy(frameOut.data() + HEADER_SIZE, ct.data(), ctLen);

    return true;
}

bool parseFrameHeader(const uint8_t* frame, size_t frameLen,
                      uint16_t& cmd, uint16_t& tid, uint16_t& crc, uint16_t& payloadLen)
{
    if (frameLen < HEADER_SIZE) return false;
    if (frame[0] != MAGIC_H || frame[1] != MAGIC_M) return false;

    cmd = (frame[2] << 8) | frame[3];
    tid = (frame[4] << 8) | frame[5];
    crc = (frame[6] << 8) | frame[7];
    uint16_t length = (frame[8] << 8) | frame[9];
    payloadLen = length - HEADER_SIZE;

    return true;
}

size_t expectedFrameSize(const uint8_t* header, size_t headerLen)
{
    if (headerLen < HEADER_SIZE) return 0;

    uint16_t cmd = (header[2] << 8) | header[3];
    uint16_t length = (header[8] << 8) | header[9];

    if (isV0Command(cmd)) {
        return length; // V0: length includes header + ciphertext, no tag
    } else {
        return length + GCM_TAG_SIZE; // V1: length + 16 byte GCM tag
    }
}

bool parseFrameV1(const uint8_t encRand[16],
                  const uint8_t* frame, size_t frameLen,
                  uint16_t& cmd, uint16_t& tid,
                  std::vector<uint8_t>& plaintext)
{
    uint16_t crc, payloadLen;
    if (!parseFrameHeader(frame, frameLen, cmd, tid, crc, payloadLen)) {
        return false;
    }

    // Check we have enough data (header + ciphertext + tag)
    if (frameLen < HEADER_SIZE + payloadLen + GCM_TAG_SIZE) {
        return false;
    }

    const uint8_t* ct = frame + HEADER_SIZE;
    const uint8_t* tag = frame + HEADER_SIZE + payloadLen;

    // Verify CRC
    uint16_t calcCrc = crc16Modbus(ct, payloadLen);
    if (calcCrc != crc) {
        return false;
    }

    // Decrypt
    plaintext.resize(payloadLen);
    if (!HiFlowCrypto::decryptV1(encRand, cmd, tid, ct, payloadLen, tag, plaintext.data())) {
        return false; // GCM tag mismatch - encRand may be stale
    }

    return true;
}

bool parseFrameV0(const char* sn,
                  const uint8_t* frame, size_t frameLen,
                  uint16_t& cmd, uint16_t& tid,
                  std::vector<uint8_t>& plaintext)
{
    uint16_t crc, payloadLen;
    if (!parseFrameHeader(frame, frameLen, cmd, tid, crc, payloadLen)) {
        return false;
    }

    if (frameLen < HEADER_SIZE + payloadLen) {
        return false;
    }

    const uint8_t* ct = frame + HEADER_SIZE;

    // Verify CRC
    uint16_t calcCrc = crc16Modbus(ct, payloadLen);
    if (calcCrc != crc) {
        return false;
    }

    // Decrypt
    plaintext.resize(payloadLen); // max possible output size
    size_t ptLen = 0;
    if (!HiFlowCrypto::decryptV0(sn, cmd, tid, ct, payloadLen, plaintext.data(), ptLen)) {
        return false;
    }
    plaintext.resize(ptLen);

    return true;
}

// ── Protobuf helpers ──────────────────────────────────────────────────────────

size_t pbEncodeVarint(uint8_t* buf, uint64_t value)
{
    size_t i = 0;
    while (value > 0x7F) {
        buf[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    buf[i++] = value & 0x7F;
    return i;
}

size_t pbEncodeVarintField(uint8_t* buf, uint8_t fieldNum, uint64_t value)
{
    size_t pos = 0;
    buf[pos++] = (fieldNum << 3) | 0; // wire type 0 = varint
    pos += pbEncodeVarint(buf + pos, value);
    return pos;
}

size_t pbEncodeBytesField(uint8_t* buf, uint8_t fieldNum, const uint8_t* data, size_t dataLen)
{
    size_t pos = 0;
    buf[pos++] = (fieldNum << 3) | 2; // wire type 2 = length-delimited
    pos += pbEncodeVarint(buf + pos, dataLen);
    memcpy(buf + pos, data, dataLen);
    pos += dataLen;
    return pos;
}

size_t pbDecodeVarint(const uint8_t* buf, size_t bufLen, uint64_t& value)
{
    value = 0;
    size_t i = 0;
    uint8_t shift = 0;
    while (i < bufLen && i < 10) {
        uint8_t b = buf[i];
        value |= (uint64_t)(b & 0x7F) << shift;
        i++;
        if ((b & 0x80) == 0) {
            return i;
        }
        shift += 7;
    }
    return 0; // error: varint too long or buffer too short
}

// Skip a field value based on wire type, return bytes consumed
static size_t pbSkipField(const uint8_t* buf, size_t bufLen, uint8_t wireType)
{
    if (wireType == 0) {
        // varint
        uint64_t dummy;
        return pbDecodeVarint(buf, bufLen, dummy);
    } else if (wireType == 2) {
        // length-delimited
        uint64_t len;
        size_t consumed = pbDecodeVarint(buf, bufLen, len);
        if (consumed == 0) return 0;
        return consumed + (size_t)len;
    } else if (wireType == 5) {
        return 4; // 32-bit
    } else if (wireType == 1) {
        return 8; // 64-bit
    }
    return 0; // unknown wire type
}

bool pbFindVarintField(const uint8_t* msg, size_t msgLen, uint8_t fieldNum, uint64_t& varintOut)
{
    size_t pos = 0;
    while (pos < msgLen) {
        uint64_t tag;
        size_t tagLen = pbDecodeVarint(msg + pos, msgLen - pos, tag);
        if (tagLen == 0) return false;
        pos += tagLen;

        uint8_t fn = (uint8_t)(tag >> 3);
        uint8_t wt = (uint8_t)(tag & 0x07);

        if (fn == fieldNum && wt == 0) {
            size_t vLen = pbDecodeVarint(msg + pos, msgLen - pos, varintOut);
            return vLen > 0;
        }

        // Skip this field
        size_t skip = pbSkipField(msg + pos, msgLen - pos, wt);
        if (skip == 0) return false;
        pos += skip;
    }
    return false;
}

bool pbFindBytesField(const uint8_t* msg, size_t msgLen, uint8_t fieldNum,
                      const uint8_t*& bytesOut, size_t& bytesLenOut)
{
    size_t pos = 0;
    while (pos < msgLen) {
        uint64_t tag;
        size_t tagLen = pbDecodeVarint(msg + pos, msgLen - pos, tag);
        if (tagLen == 0) return false;
        pos += tagLen;

        uint8_t fn = (uint8_t)(tag >> 3);
        uint8_t wt = (uint8_t)(tag & 0x07);

        if (fn == fieldNum && wt == 2) {
            uint64_t len;
            size_t lenBytes = pbDecodeVarint(msg + pos, msgLen - pos, len);
            if (lenBytes == 0) return false;
            pos += lenBytes;
            bytesOut = msg + pos;
            bytesLenOut = (size_t)len;
            return true;
        }

        // Skip this field
        size_t skip = pbSkipField(msg + pos, msgLen - pos, wt);
        if (skip == 0) return false;
        pos += skip;
    }
    return false;
}

} // namespace HiFlowProtocol
