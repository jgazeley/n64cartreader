/**
 * @file format.h
 * @brief Public API for shared text and data formatting utilities.
 */

#ifndef UTILS_FORMAT_H
#define UTILS_FORMAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/// Default bytes per line for hex dump
#define FORMAT_HEXDUMP_DEFAULT_BYTES_PER_LINE 16u
/// Default max rows (0 = no limit)
#define FORMAT_HEXDUMP_DEFAULT_MAX_ROWS       0u

/**
 * @brief Options controlling how a hex dump is rendered.
 */
typedef struct {
    uint32_t bytes_per_line;  /**< Bytes per row (columns) */
    uint32_t max_rows;        /**< Max rows to output (0 = unlimited) */
    bool     show_ascii;      /**< Include ASCII gutter if true */
} hexdump_options_t;

/**
 * @brief Dumps a buffer to stdout as a formatted hex and ASCII view.
 *
 * @param data      Pointer to the data buffer.
 * @param length    Number of bytes to display.
 * @param base_addr Base offset to print for each line.
 */

void utils_format_hexdump(const void *data,
                           size_t length,
                           uint32_t base_addr);

/**
 * @brief Extended hex dump with formatting options.
 *
 * @param data      Pointer to the data buffer.
 * @param length    Number of bytes to display.
 * @param base_addr Base offset to print for each line.
 * @param opts      Pointer to formatting options (NULL = defaults).
 */
void utils_format_hexdump_ex(const void *data,
                              size_t length,
                              uint32_t base_addr,
                              const hexdump_options_t *opts);


// utils_format_hexdump( * -- * )
// --- N64 ROM Header @ 0x10000000 ---
// Offset (h)  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  Decoded text

// 0x10000000: 80 37 12 40 00 00 00 0F 80 00 04 00 00 00 14 47  .7.@...........G
// 0x10000010: DC BC 50 D1 09 FD 1A A3 00 00 00 00 00 00 00 00  ..P.............
// 0x10000020: 47 4F 4C 44 45 4E 45 59 45 20 20 20 20 20 20 20  GOLDENEYE
// 0x10000030: 20 20 20 20 00 00 00 00 00 00 00 4E 47 45 45 00      .......NGEE.







/**
 * @brief Dumps a buffer as plain hex bytes, wrapping after a fixed count.
 *
 * @param data           Pointer to the data buffer.
 * @param length         Number of bytes to display.
 * @param bytes_per_line How many bytes to print before inserting a newline.
 */
void utils_format_hex_lines(const void *data,
                            size_t length,
                            uint32_t bytes_per_line);



#endif // UTILS_FORMAT_H