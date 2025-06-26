// File: utils/crc.h
#ifndef UTILS_CRC_H
#define UTILS_CRC_H

#include <stddef.h>
#include <stdint.h>

// Toggle table-driven (fast) vs bitwise (slow) implementations:
#ifndef USE_CRC_FAST
#  define USE_CRC_FAST 1
#endif

// in utils/crc.h
uint16_t crc16_update(uint16_t seed, const uint8_t *data, size_t len);

/**
 * Bitwise CRC-16-IBM (slow).
 * One-shot: seeds 0x0000, returns raw CRC.
 */
uint16_t crc16(const uint8_t *data, size_t len);

/**
 * Table-driven CRC-16-IBM (fast).
 * One-shot: seeds 0x0000, returns raw CRC.
 */
uint16_t crc16_fast(const uint8_t *data, size_t len);

/**
 * Bitwise CRC-32/IEEE (slow).
 * One-shot: seeds 0xFFFFFFFF, returns inverted CRC.
 */
uint32_t crc32(const uint8_t *data, size_t len);

/**
 * Table-driven CRC-32/IEEE (fast).
 * One-shot: seeds 0xFFFFFFFF, returns inverted CRC.
 */
uint32_t crc32_fast(const uint8_t *data, size_t len);

/**
 * Default aliases:
 *  - When USE_CRC_FAST=1, crc16_ibm→crc16_fast, crc32_ieee→crc32_fast
 *  - Otherwise, uses crc16/bitwise and crc32/bitwise
 */
#if USE_CRC_FAST
#  define crc16_ibm   crc16_fast
#  define crc32_ieee  crc32_fast
#else
#  define crc16_ibm   crc16
#  define crc32_ieee  crc32
#endif

#endif // UTILS_CRC_H