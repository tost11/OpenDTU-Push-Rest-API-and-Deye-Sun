// SPDX-License-Identifier: GPL-2.0-or-later
#include "Base64.h"

constexpr const char* Base64::BASE64_CHARS;

int Base64::encode(unsigned char* dst, size_t dst_len, size_t* olen,
                   const unsigned char* src, size_t src_len)
{
    // Check if output buffer is large enough
    size_t required = encodedLength(src_len);
    if (dst_len < required) {
        return -1;
    }

    size_t i = 0;
    size_t j = 0;

    // Process 3 bytes at a time
    while (i + 2 < src_len) {
        dst[j++] = BASE64_CHARS[(src[i] >> 2) & 0x3F];
        dst[j++] = BASE64_CHARS[((src[i] & 0x03) << 4) | ((src[i + 1] >> 4) & 0x0F)];
        dst[j++] = BASE64_CHARS[((src[i + 1] & 0x0F) << 2) | ((src[i + 2] >> 6) & 0x03)];
        dst[j++] = BASE64_CHARS[src[i + 2] & 0x3F];
        i += 3;
    }

    // Handle remaining bytes
    if (i < src_len) {
        dst[j++] = BASE64_CHARS[(src[i] >> 2) & 0x3F];

        if (i + 1 < src_len) {
            // 2 bytes remaining
            dst[j++] = BASE64_CHARS[((src[i] & 0x03) << 4) | ((src[i + 1] >> 4) & 0x0F)];
            dst[j++] = BASE64_CHARS[((src[i + 1] & 0x0F) << 2)];
            dst[j++] = '=';
        } else {
            // 1 byte remaining
            dst[j++] = BASE64_CHARS[((src[i] & 0x03) << 4)];
            dst[j++] = '=';
            dst[j++] = '=';
        }
    }

    dst[j] = '\0';
    *olen = j + 1;  // Include null terminator

    return 0;
}
