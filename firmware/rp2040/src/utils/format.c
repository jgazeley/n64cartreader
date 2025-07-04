#include "utils/format.h"

#include <stdio.h>
#include <ctype.h>

void utils_format_hexdump(const void *data,
                           size_t length,
                           uint32_t base_addr)
{
    hexdump_options_t def = {
        .bytes_per_line = FORMAT_HEXDUMP_DEFAULT_BYTES_PER_LINE,
        .max_rows       = FORMAT_HEXDUMP_DEFAULT_MAX_ROWS,
        .show_ascii     = true
    };
    utils_format_hexdump_ex(data, length, base_addr, &def);
}

void utils_format_hexdump_ex(const void *data,
                              size_t length,
                              uint32_t base_addr,
                              const hexdump_options_t *opts)
{
    const uint8_t *bytes = (const uint8_t *)data;
    // Determine parameters (fall back to defaults)
    uint32_t bpl  = opts && opts->bytes_per_line  ? opts->bytes_per_line  : FORMAT_HEXDUMP_DEFAULT_BYTES_PER_LINE;
    uint32_t maxr = opts && opts->max_rows        ? opts->max_rows        : FORMAT_HEXDUMP_DEFAULT_MAX_ROWS;
    bool     asc  = opts ? opts->show_ascii : true;

    // Print header row
    printf("\nOffset (h)  ");
    for (uint32_t j = 0; j < bpl; j++) {
        printf("%02X ", j);
    }
    if (asc) {
        printf(" Decoded text");
    }
    printf("\n\n");

    // Iterate through the data in lines of bpl bytes
    uint32_t row = 0;
    for (size_t i = 0; i < length; i += bpl) {
        if (maxr && row++ >= maxr) break;

        // Print the base address for the current line
        printf("0x%08X: ", base_addr + (uint32_t)i);

        // Hex bytes
        size_t line_len = (i + bpl <= length ? bpl : length - i);
        for (uint32_t j = 0; j < bpl; j++) {
            if (j < line_len) {
                printf("%02X ", bytes[i + j]);
            } else {
                printf("   ");
            }
        }

        // ASCII gutter
        if (asc) {
            printf(" ");
            for (uint32_t j = 0; j < line_len; j++) {
                unsigned char c = bytes[i + j];
                putchar(isprint(c) ? c : '.');
            }
        }

        printf("\n");
    }
}

void utils_format_hex_lines(const void *data,
                            size_t length,
                            uint32_t bytes_per_line)
{
    const uint8_t *bytes = (const uint8_t *)data;
    size_t col = 0;

    for (size_t i = 0; i < length; ++i) {
        // Print the byte
        printf("%02X", bytes[i]);

        // Newline after bytes_per_line of them
        if (++col >= bytes_per_line) {
            putchar('\n');
            col = 0;
        }
    }

    // Final newline if we ended mid-line
    if (col) {
        putchar('\n');
    }
}