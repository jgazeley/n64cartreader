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

#endif // UTILS_FORMAT_H