#pragma once

#include <cstdint>
#include <cstddef>

/**
 * Cryptographic utilities for HiFlow BLE protocol.
 * 
 * V0 (pairing): AES-128-CBC with SN-derived key/IV
 * V1 (session): AES-128-GCM with encRand-derived key/nonce
 */
namespace HiFlowCrypto {

// Salt appended to SN for V0 key derivation
static constexpr const char* SALT_V0 = "Hoymiles@#123456";

/**
 * Triple SHA-256: SHA256(SHA256(SHA256(input)))
 * @param input Input bytes
 * @param inputLen Length of input
 * @param output 32-byte output buffer
 */
void tripleSha256(const uint8_t* input, size_t inputLen, uint8_t output[32]);

// ── V0: SN-keyed AES-128-CBC ──────────────────────────────────────

/**
 * Derive 16-byte AES key for V0 from inverter serial number.
 * Key = tripleSHA256(sn + "Hoymiles@#123456")[:16]
 */
void deriveV0Key(const char* sn, uint8_t key[16]);

/**
 * Derive 16-byte CBC IV for V0.
 * IV = tripleSHA256(BE(cmd, tid) + sn)[16:32]
 */
void deriveV0Iv(const char* sn, uint16_t cmd, uint16_t tid, uint8_t iv[16]);

/**
 * Encrypt plaintext with V0 (AES-128-CBC + PKCS7 padding).
 * @param sn 12-char serial
 * @param cmd Command code
 * @param tid Transaction ID
 * @param plaintext Input data
 * @param plaintextLen Length of plaintext
 * @param ciphertext Output buffer (must be at least plaintextLen + 16 bytes for padding)
 * @param ciphertextLen Output: actual length of ciphertext
 * @return true on success
 */
bool encryptV0(const char* sn, uint16_t cmd, uint16_t tid,
               const uint8_t* plaintext, size_t plaintextLen,
               uint8_t* ciphertext, size_t& ciphertextLen);

/**
 * Decrypt ciphertext with V0 (AES-128-CBC + PKCS7 unpadding).
 * @param sn 12-char serial
 * @param cmd Command code
 * @param tid Transaction ID
 * @param ciphertext Input data
 * @param ciphertextLen Length of ciphertext (must be multiple of 16)
 * @param plaintext Output buffer (same size as ciphertext is safe)
 * @param plaintextLen Output: actual length after unpadding
 * @return true on success
 */
bool decryptV0(const char* sn, uint16_t cmd, uint16_t tid,
               const uint8_t* ciphertext, size_t ciphertextLen,
               uint8_t* plaintext, size_t& plaintextLen);

// ── V1: encRand-keyed AES-128-GCM ─────────────────────────────────

/**
 * Derive 16-byte AES key for V1.
 * Key = tripleSHA256(encRand)[:16]
 */
void deriveV1Key(const uint8_t encRand[16], uint8_t key[16]);

/**
 * Derive 12-byte GCM nonce for V1.
 * Nonce = tripleSHA256(LE(cmd, tid) + encRand)[20:32]
 */
void deriveV1Nonce(const uint8_t encRand[16], uint16_t cmd, uint16_t tid, uint8_t nonce[12]);

/**
 * Build 4-byte AAD for V1: LE(cmd) || LE(tid)
 */
void buildV1Aad(uint16_t cmd, uint16_t tid, uint8_t aad[4]);

/**
 * Encrypt plaintext with V1 (AES-128-GCM).
 * @param encRand 16-byte device key
 * @param cmd Command code
 * @param tid Transaction ID
 * @param plaintext Input
 * @param plaintextLen Length
 * @param ciphertext Output (same length as plaintext)
 * @param tag 16-byte GCM authentication tag output
 * @return true on success
 */
bool encryptV1(const uint8_t encRand[16], uint16_t cmd, uint16_t tid,
               const uint8_t* plaintext, size_t plaintextLen,
               uint8_t* ciphertext, uint8_t tag[16]);

/**
 * Decrypt ciphertext with V1 (AES-128-GCM).
 * @param encRand 16-byte device key
 * @param cmd Command code
 * @param tid Transaction ID
 * @param ciphertext Input
 * @param ciphertextLen Length
 * @param tag 16-byte GCM authentication tag to verify
 * @param plaintext Output (same length as ciphertext)
 * @return true on success (tag verified), false if tag mismatch (encRand stale)
 */
bool decryptV1(const uint8_t encRand[16], uint16_t cmd, uint16_t tid,
               const uint8_t* ciphertext, size_t ciphertextLen,
               const uint8_t tag[16], uint8_t* plaintext);

} // namespace HiFlowCrypto
