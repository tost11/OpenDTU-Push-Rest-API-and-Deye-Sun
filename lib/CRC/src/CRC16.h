// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "CRC.h"

// Modbus CRC16 configuration constants (for compatibility)
#define CRC16_MODBUS_INITIAL 0xFFFF
#define CRC16_MODBUS_POLYNOME 0xA001
#define CRC16_MODBUS_REV_IN true
#define CRC16_MODBUS_REV_OUT true
#define CRC16_MODBUS_XOR_OUT 0x0000

/**
 * @brief CRC16 class wrapper for Hoymiles-W compatibility
 *
 * This class provides a robtillaart/CRC-compatible interface
 * around the shared crc16() function. All configuration methods
 * are no-ops since we use hardcoded Modbus CRC16.
 */
class CRC16 {
private:
    uint16_t _crc;

public:
    CRC16() : _crc(CRC16_MODBUS_INITIAL) {}

    /**
     * @brief Restart CRC calculation
     */
    void restart() {
        _crc = CRC16_MODBUS_INITIAL;
    }

    /**
     * @brief Add a byte to the CRC calculation
     * @param b Byte to add
     */
    void add(uint8_t b) {
        _crc = crc16(&b, 1, _crc);
    }

    /**
     * @brief Get current CRC value
     * @return CRC16 value
     */
    uint16_t calc() const {
        return _crc;
    }

    // Configuration methods (no-op, Modbus config is hardcoded)
    void setInitial(uint16_t) {}
    void setPolynome(uint16_t) {}
    void setReverseIn(bool) {}
    void setReverseOut(bool) {}
    void setXorOut(uint16_t) {}
};
