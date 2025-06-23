#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"

#include <bus/ad_bus.h>             /* 16-bit multiplexed bus */
#include <bus/joybus.h>             /* 1-wire serial + clock  */
#include <devices/cartridge.h>

// N64 ROM constants
#define N64_FAST_CHUNK_BYTES 1024u    // 1 KiB burst
#define N64_FAST_CHUNK_WORDS (N64_FAST_CHUNK_BYTES/2)

// Primitive byte reader: reads 'len' even bytes starting at base_addr
bool n64_read_bytes(uint32_t base_addr, uint8_t *buf, size_t len) {
    if (!buf || (len & 1)) return false;   // length must be even
    for (size_t i = 0; i < len; i += 2) {
        adBus_set_address(base_addr + i);
        uint16_t w = n64_read16();
        buf[i]   = (uint8_t)(w >> 8);
        buf[i+1] = (uint8_t)(w & 0xFF);
    }
    return true;
}

// Read larger chunks of data
bool n64_read_bytes_fast(uint32_t base_addr, uint8_t *buf, size_t len) {
    if (!buf || (len & 1)) return false;
    while (len > 0) {
        // how many bytes to do in this chunk?
        size_t chunk = (len < N64_FAST_CHUNK_BYTES ? len : N64_FAST_CHUNK_BYTES);
        size_t words = chunk / 2;

        // 1) latch the start address for this burst
        adBus_set_address(base_addr);
        adBus_dir(false);  // release bus to the cartridge

        // 2) read 'words' sequential 16-bit values
        for (size_t i = 0; i < words; ++i) {
            uint16_t w = n64_read16();      // toggles /RD, waits, samples
            buf[2*i]   = (uint8_t)(w >> 8);   // MSB
            buf[2*i+1] = (uint8_t)(w & 0xFF); // LSB
        }

        // advance pointers
        base_addr += chunk;
        buf       += chunk;
        len       -= chunk;
    }
    return true;
}

// Read the 64-byte ROM header
bool n64_get_header(uint8_t *buffer, size_t buffer_size) {
    if (!buffer || buffer_size < N64_HEADER_LENGTH) return false;
    return n64_read_bytes(N64_ROM_BASE, buffer, N64_HEADER_LENGTH);
}

// Read the 20-byte title field, sanitize, trim trailing spaces, and apply a "no cart" check
bool n64_get_title(uint8_t *buffer, size_t buffer_size) {
    if (!buffer || buffer_size < (N64_TITLE_LENGTH + 1)) return false;

    uint8_t raw[N64_TITLE_LENGTH];
    if (!n64_read_bytes(N64_ROM_BASE + N64_TITLE_OFFSET,
                        raw,
                        N64_TITLE_LENGTH)) {
        return false;
    }

    // Copy & sanitize into buffer
    for (size_t i = 0; i < N64_TITLE_LENGTH; ++i) {
        char c = (char)raw[i];
        buffer[i] = (isprint((unsigned char)c) ? c : ' ');
    }

    // Trim trailing spaces
    int end = N64_TITLE_LENGTH - 1;
    while (end >= 0 && buffer[end] == ' ') {
        --end;
    }
    buffer[end + 1] = '\0';  // null-terminate right after last non-space

    // Heuristic: read the first two words of the title area
    adBus_set_address(N64_ROM_BASE + N64_TITLE_OFFSET);
    uint16_t w1 = n64_read16();
    adBus_set_address(N64_ROM_BASE + N64_TITLE_OFFSET + 2);
    uint16_t w2 = n64_read16();
    if ((w1 == 0x0000 && w2 == 0x0000) ||
        (w1 == 0xFFFF && w2 == 0xFFFF)) {
        // No valid cart or blank header → substitute placeholder
        strncpy((char*)buffer, "NOCART", buffer_size);
        buffer[buffer_size - 1] = '\0';
        return false;
    }

    return true;
}