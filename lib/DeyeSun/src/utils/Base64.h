// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Lightweight base64 encoder for HTTP Basic Auth
 *
 * Minimal implementation optimized for small strings (username:password).
 * Saves flash space compared to mbedtls base64 implementation.
 */
class Base64 {
public:
    /**
     * @brief Encode binary data to base64 string
     * @param dst Output buffer (must be at least Base64::encodedLength(src_len) bytes)
     * @param dst_len Size of output buffer
     * @param olen Pointer to store actual encoded length (including null terminator)
     * @param src Input data
     * @param src_len Length of input data
     * @return 0 on success, -1 if output buffer too small
     */
    static int encode(unsigned char* dst, size_t dst_len, size_t* olen,
                     const unsigned char* src, size_t src_len);

    /**
     * @brief Calculate required output buffer size for encoding
     * @param src_len Length of input data
     * @return Required buffer size (including null terminator)
     */
    static constexpr size_t encodedLength(size_t src_len) {
        return ((src_len + 2) / 3) * 4 + 1;  // +1 for null terminator
    }

private:
    static constexpr const char* BASE64_CHARS =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
};
