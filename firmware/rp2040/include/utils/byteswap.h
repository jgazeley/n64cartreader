/**
 * @file byteswap.h
 * @brief Utilities for byte order conversion (endianness).
 *
 * Provides functions to swap the byte order of multi-byte integers.
 * Useful when interfacing with hardware or protocols that use a
 * different endianness than the host processor.
 */
#ifndef UTILS_BYTESWAP_H
#define UTILS_BYTESWAP_H

#include <stdint.h> // For uint16_t, uint32_t, uint64_t

/**
 * @brief Swaps the byte order of a 16-bit unsigned integer.
 * Example: 0xABCD becomes 0xCDBA
 * @param val The 16-bit value to swap.
 * @return The byte-swapped 16-bit value.
 */
static inline uint16_t byteswap16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

/**
 * @brief Swaps the byte order of a 32-bit unsigned integer.
 * Example: 0xAABBCCDD becomes 0xDDCCBBAA
 * @param val The 32-bit value to swap.
 * @return The byte-swapped 32-bit value.
 */
static inline uint32_t byteswap32(uint32_t val) {
    return ((val << 24) & 0xFF000000) |
           ((val << 8)  & 0x00FF0000) |
           ((val >> 8)  & 0x0000FF00) |
           ((val >> 24) & 0x000000FF);
}

/**
 * @brief Swaps the byte order of a 64-bit unsigned integer.
 * @param val The 64-bit value to swap.
 * @return The byte-swapped 64-bit value.
 */
static inline uint64_t byteswap64(uint64_t val) {
    return ((val << 56) & 0xFF00000000000000ULL) |
           ((val << 40) & 0x00FF000000000000ULL) |
           ((val << 24) & 0x0000FF0000000000ULL) |
           ((val << 8)  & 0x000000FF00000000ULL) |
           ((val >> 8)  & 0x00000000FF000000ULL) |
           ((val >> 24) & 0x0000000000FF0000ULL) |
           ((val >> 40) & 0x000000000000FF00ULL) |
           ((val >> 56) & 0x00000000000000FFULL);
}

// You might also add functions for host-to-big/little endian if needed:
// Example:
// #define IS_LITTLE_ENDIAN (1 == *(unsigned char *)&(const int){1})
// #define HTONS(x) (IS_LITTLE_ENDIAN ? byteswap16(x) : (x))
// #define NTOHS(x) (IS_LITTLE_ENDIAN ? byteswap16(x) : (x))

static inline void byteswap16_buf(uint16_t *buf, size_t count) {
    for (size_t i = 0; i < count; ++i)
        buf[i] = byteswap16(buf[i]);
}
static inline void byteswap32_buf(uint32_t *buf, size_t count) {
    for (size_t i = 0; i < count; ++i)
        buf[i] = byteswap32(buf[i]);
}

#endif // UTILS_BYTESWAP_H