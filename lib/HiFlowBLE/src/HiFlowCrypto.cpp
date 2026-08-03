#include "HiFlowCrypto.h"
#include <cstring>
#include <mbedtls/sha256.h>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>

namespace HiFlowCrypto {

void tripleSha256(const uint8_t* input, size_t inputLen, uint8_t output[32])
{
    uint8_t buf1[32], buf2[32];

    // First SHA-256
    mbedtls_sha256(input, inputLen, buf1, 0);
    // Second SHA-256
    mbedtls_sha256(buf1, 32, buf2, 0);
    // Third SHA-256
    mbedtls_sha256(buf2, 32, output, 0);
}

// ── V0 ────────────────────────────────────────────────────────────────────────

void deriveV0Key(const char* sn, uint8_t key[16])
{
    // key = tripleSHA256(sn.encode() + SALT_V0)[:16]
    size_t snLen = strlen(sn);
    size_t saltLen = strlen(SALT_V0);
    size_t totalLen = snLen + saltLen;

    uint8_t buf[64]; // sn (12) + salt (16) = 28, well within 64
    memcpy(buf, sn, snLen);
    memcpy(buf + snLen, SALT_V0, saltLen);

    uint8_t hash[32];
    tripleSha256(buf, totalLen, hash);
    memcpy(key, hash, 16);
}

void deriveV0Iv(const char* sn, uint16_t cmd, uint16_t tid, uint8_t iv[16])
{
    // IV = tripleSHA256( pack(">HH", cmd, tid) + sn.encode() )[16:32]
    size_t snLen = strlen(sn);
    uint8_t buf[64]; // 4 + 12 = 16, within 64

    // Big-endian pack of cmd and tid
    buf[0] = (cmd >> 8) & 0xFF;
    buf[1] = cmd & 0xFF;
    buf[2] = (tid >> 8) & 0xFF;
    buf[3] = tid & 0xFF;
    memcpy(buf + 4, sn, snLen);

    uint8_t hash[32];
    tripleSha256(buf, 4 + snLen, hash);
    memcpy(iv, hash + 16, 16); // bytes [16:32]
}

bool encryptV0(const char* sn, uint16_t cmd, uint16_t tid,
               const uint8_t* plaintext, size_t plaintextLen,
               uint8_t* ciphertext, size_t& ciphertextLen)
{
    uint8_t key[16], iv[16];
    deriveV0Key(sn, key);
    deriveV0Iv(sn, cmd, tid, iv);

    // PKCS7 padding
    uint8_t padLen = 16 - (plaintextLen % 16);
    ciphertextLen = plaintextLen + padLen;

    // Create padded plaintext
    uint8_t* padded = new uint8_t[ciphertextLen];
    memcpy(padded, plaintext, plaintextLen);
    memset(padded + plaintextLen, padLen, padLen);

    // AES-128-CBC encrypt
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int ret = mbedtls_aes_setkey_enc(&aes, key, 128);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        delete[] padded;
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, ciphertextLen, iv, padded, ciphertext);
    mbedtls_aes_free(&aes);
    delete[] padded;

    return ret == 0;
}

bool decryptV0(const char* sn, uint16_t cmd, uint16_t tid,
               const uint8_t* ciphertext, size_t ciphertextLen,
               uint8_t* plaintext, size_t& plaintextLen)
{
    if (ciphertextLen == 0 || (ciphertextLen % 16) != 0) {
        return false;
    }

    uint8_t key[16], iv[16];
    deriveV0Key(sn, key);
    deriveV0Iv(sn, cmd, tid, iv);

    // AES-128-CBC decrypt
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int ret = mbedtls_aes_setkey_dec(&aes, key, 128);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, ciphertextLen, iv, ciphertext, plaintext);
    mbedtls_aes_free(&aes);

    if (ret != 0) {
        return false;
    }

    // Remove PKCS7 padding
    uint8_t padLen = plaintext[ciphertextLen - 1];
    if (padLen == 0 || padLen > 16) {
        return false;
    }
    // Verify padding bytes
    for (size_t i = ciphertextLen - padLen; i < ciphertextLen; i++) {
        if (plaintext[i] != padLen) {
            return false;
        }
    }
    plaintextLen = ciphertextLen - padLen;
    return true;
}

// ── V1 ────────────────────────────────────────────────────────────────────────

void deriveV1Key(const uint8_t encRand[16], uint8_t key[16])
{
    // key = tripleSHA256(encRand)[:16]
    uint8_t hash[32];
    tripleSha256(encRand, 16, hash);
    memcpy(key, hash, 16);
}

void deriveV1Nonce(const uint8_t encRand[16], uint16_t cmd, uint16_t tid, uint8_t nonce[12])
{
    // nonce = tripleSHA256( pack("<HH", cmd, tid) + encRand )[20:32]
    uint8_t buf[20]; // 4 + 16

    // Little-endian pack of cmd and tid
    buf[0] = cmd & 0xFF;
    buf[1] = (cmd >> 8) & 0xFF;
    buf[2] = tid & 0xFF;
    buf[3] = (tid >> 8) & 0xFF;
    memcpy(buf + 4, encRand, 16);

    uint8_t hash[32];
    tripleSha256(buf, 20, hash);
    memcpy(nonce, hash + 20, 12); // bytes [20:32]
}

void buildV1Aad(uint16_t cmd, uint16_t tid, uint8_t aad[4])
{
    // AAD = LE(cmd) || LE(tid)
    aad[0] = cmd & 0xFF;
    aad[1] = (cmd >> 8) & 0xFF;
    aad[2] = tid & 0xFF;
    aad[3] = (tid >> 8) & 0xFF;
}

bool encryptV1(const uint8_t encRand[16], uint16_t cmd, uint16_t tid,
               const uint8_t* plaintext, size_t plaintextLen,
               uint8_t* ciphertext, uint8_t tag[16])
{
    uint8_t key[16], nonce[12], aad[4];
    deriveV1Key(encRand, key);
    deriveV1Nonce(encRand, cmd, tid, nonce);
    buildV1Aad(cmd, tid, aad);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 128);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }

    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                     plaintextLen,
                                     nonce, 12,
                                     aad, 4,
                                     plaintext, ciphertext,
                                     16, tag);
    mbedtls_gcm_free(&gcm);
    return ret == 0;
}

bool decryptV1(const uint8_t encRand[16], uint16_t cmd, uint16_t tid,
               const uint8_t* ciphertext, size_t ciphertextLen,
               const uint8_t tag[16], uint8_t* plaintext)
{
    uint8_t key[16], nonce[12], aad[4];
    deriveV1Key(encRand, key);
    deriveV1Nonce(encRand, cmd, tid, nonce);
    buildV1Aad(cmd, tid, aad);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 128);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }

    ret = mbedtls_gcm_auth_decrypt(&gcm,
                                    ciphertextLen,
                                    nonce, 12,
                                    aad, 4,
                                    tag, 16,
                                    ciphertext, plaintext);
    mbedtls_gcm_free(&gcm);
    return ret == 0; // returns non-zero if tag verification fails
}

} // namespace HiFlowCrypto
