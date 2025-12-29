// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

#define CRC8_INIT 0x00
#define CRC8_POLY 0x01

#define CRC16_MODBUS_POLYNOM 0xA001
#define CRC16_NRF24_POLYNOM 0x1021

/**
 * @brief Calculate CRC8 checksum
 * @param buf Data buffer
 * @param len Length of data
 * @return CRC8 checksum
 */
uint8_t crc8(const uint8_t buf[], const uint8_t len);

/**
 * @brief Calculate CRC16 Modbus checksum
 * Used by: Hoymiles NRF24, Deye Sun, Hoymiles W-Series
 * @param buf Data buffer
 * @param len Length of data
 * @param start Initial CRC value (default 0xFFFF)
 * @return CRC16 Modbus checksum
 */
uint16_t crc16(const uint8_t buf[], const uint8_t len, const uint16_t start = 0xFFFF);

/**
 * @brief Calculate CRC16 for NRF24 radio (bit-level CRC)
 * @param buf Data buffer
 * @param lenBits Length in bits
 * @param startBit Starting bit position
 * @param crcIn Initial CRC value
 * @return CRC16 NRF24 checksum
 */
uint16_t crc16nrf24(const uint8_t buf[], const uint16_t lenBits, const uint16_t startBit = 0, const uint16_t crcIn = 0xFFFF);
