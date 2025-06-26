// File: utils/crc.c
#include "utils/crc.h"
#include <stdbool.h>

// —— CRC-16-IBM Bitwise (slow) ——
uint16_t crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0x0000u;
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 0x8000u)
                ? (uint16_t)((crc << 1) ^ 0x8005u)
                : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

// —— CRC-16-IBM Table-Driven (fast) ——
uint16_t crc16_fast(const uint8_t *data, size_t len) {
    static uint16_t table[256];
    static bool initialized = false;
    if (!initialized) {
        for (uint16_t i = 0; i < 256; ++i) {
            uint16_t r = (uint16_t)(i << 8);
            for (int j = 0; j < 8; ++j) {
                r = (r & 0x8000u)
                    ? (uint16_t)((r << 1) ^ 0x8005u)
                    : (uint16_t)(r << 1);
            }
            table[i] = r;
        }
        initialized = true;
    }
    uint16_t crc = 0x0000u;
    for (size_t k = 0; k < len; ++k) {
        uint8_t idx = (uint8_t)(crc >> 8) ^ data[k];
        crc = (uint16_t)(table[idx] ^ (crc << 8));
    }
    return crc;
}

uint16_t crc16_update(uint16_t crc, const uint8_t *data, size_t len) {
    #if USE_CRC_FAST
    // reuse your table-driven code, except seed with `crc` not 0
    // (just inline the same loop as crc16_fast but initialize `crc` from the arg)
    #else
    // or bitwise:
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 0x8000u)
                ? (uint16_t)((crc << 1) ^ 0x8005u)
                : (uint16_t)(crc << 1);
        }
    }
    #endif
    return crc;
}

// —— CRC-32/IEEE Bitwise (slow) ——
uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 1u)
                ? (crc >> 1) ^ 0xEDB88320u
                : (crc >> 1);
        }
    }
    return ~crc;
}

// —— CRC-32/IEEE Table-Driven (fast) ——
uint32_t crc32_fast(const uint8_t *data, size_t len) {
    static uint32_t table[256];
    static bool initialized = false;
    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t r = i;
            for (int j = 0; j < 8; ++j) {
                r = (r & 1u)
                    ? (r >> 1) ^ 0xEDB88320u
                    : (r >> 1);
            }
            table[i] = r;
        }
        initialized = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t k = 0; k < len; ++k) {
        crc = table[(uint8_t)crc ^ data[k]] ^ (crc >> 8);
    }
    return ~crc;
}