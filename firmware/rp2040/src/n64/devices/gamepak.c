/**
 * @file gamepak.c
 * @brief GamePak (cartridge) interface implementation.
 *
 * Implements functions to initialize the GamePak subsystem and
 * read bytes from the ROM via the AD-bus.
 */
#include "pico/stdlib.h"
#include <stddef.h>
#include <string.h>

#include "n64/devices/gamepak.h"
#include "n64/bus/adbus.h"
#include "n64/bus/joybus.h"

static n64_gamepak_info_t gamepak_info = {
    .valid = false,
    .save_type = N64_SAVE_TYPE_NONE
};

// single 512-byte save buffer
static uint8_t save_buf[N64_GAMEPAK_SRAM_PAGE_SIZE];

bool gamepak_init(void) {
    // 1) Init the AD-bus
    if (!adbus_init()) {
        gamepak_info.valid = false;
        return false;
    }

    // if (!n64_joybus_init()) {
    //     // gamepak_info.valid = false;
    //     return false;
    // }
    n64_joybus_init();

    // 2) Reset & settle bus
    n64_adbus_reset();
    sleep_ms(5);

    // 3) Read the 64-byte ROM header straight into our struct
    uint8_t *hdr_dst = (uint8_t *)&gamepak_info.header;
    n64_adbus_set_direction(false);
    for (size_t w = 0; w < (N64_GAMEPAK_HEADER_SIZE / 2); ++w) {
        uint32_t addr = N64_GAMEPAK_ROM_BASE + (w * 2);
        n64_adbus_set_address(addr);
        uint16_t word = n64_adbus_read16();
        hdr_dst[w*2    ] = (uint8_t)(word >> 8);
        hdr_dst[w*2 + 1] = (uint8_t)(word & 0xFF);
    }
    // — no manual null-termination here! use gamepak_get_title() instead.

    // 4) Probe for SRAM and preload first page if present
    if (gamepak_has_sram()) {
        gamepak_info.save_type = N64_SAVE_TYPE_SRAM;
        // read first 512-byte page into save_buf
        if (!gamepak_read_sram_page(N64_GAMEPAK_SRAM_BASE, save_buf)) {
            // if it fails, clear the save_type
            gamepak_info.save_type = N64_SAVE_TYPE_NONE;
        }
    }

    // 5) Mark everything valid
    gamepak_info.valid = true;
    return true;
}

const n64_gamepak_info_t *gamepak_get_info(void) {
    return gamepak_info.valid ? &gamepak_info : NULL;
}

const n64_gamepak_header_t *gamepak_get_header(void) {
    // Return the header pointer only if the overall info is valid
    return gamepak_info.valid ? &gamepak_info.header : NULL;
}

void gamepak_read_header(uint8_t *buffer) {

    // ensure bus initialized & idle
    // n64_adbus_reset();
    // make sure bus is in read mode
    adbus_set_direction(false);

    // read N64_LENGTH_HEADER bytes as 16-bit words
    for (size_t w = 0; w < (N64_GAMEPAK_HEADER_SIZE / 2); ++w) {
        uint32_t addr = N64_GAMEPAK_ROM_BASE + (uint32_t)(w * 2);
        adbus_latch_address(addr);

        // bus returns big-endian 16 bits
        uint16_t word = adbus_read_word();
        buffer[w*2 + 0] = (uint8_t)(word >> 8);
        buffer[w*2 + 1] = (uint8_t)(word & 0xFF);
    }
}

bool gamepak_is_valid(void) {
    return gamepak_info.valid;
}

uint32_t gamepak_get_crc1(void) {
    return gamepak_info.header.crc1;
}

uint32_t gamepak_get_crc2(void) {
    return gamepak_info.header.crc2;
}

const char *gamepak_get_title(void) {
    static char title_str[N64_GAMEPAK_TITLE_LENGTH + 1];
    size_t len = N64_GAMEPAK_TITLE_LENGTH;

    // Copy exactly 20 bytes from the packed header
    memcpy(title_str,
           gamepak_info.header.title,
           len);

    // Trim any trailing spaces
    while (len > 0 && title_str[len - 1] == ' ') {
        --len;
    }

    // NUL-terminate just after the last non-space character
    title_str[len] = '\0';

    return title_str;
}

uint8_t gamepak_get_country_code(void) {
    return gamepak_info.header.country_code;
}

uint8_t gamepak_get_version(void) {
    return gamepak_info.header.version;
}

bool gamepak_read_bytes(uint32_t base_addr, uint8_t *buf, size_t len) {
    if (!buf || (len & 1)) return false;   // length must be even
    for (size_t i = 0; i < len; i += 2) {
        n64_adbus_set_address(base_addr + i);
        uint16_t w = n64_adbus_read16();
        buf[i]   = (uint8_t)(w >> 8);
        buf[i+1] = (uint8_t)(w & 0xFF);
    }
    return true;
}

bool gamepak_read_bytes_fast(uint32_t base_addr, uint8_t *buf, size_t len) {
    // if (!buf || (len & 1)) return false;
    // while (len > 0) {
    //     // how many bytes to do in this chunk?
    //     size_t chunk = (len < N64_FAST_CHUNK_BYTES ? len : N64_FAST_CHUNK_BYTES);
    //     size_t words = chunk / 2;

    //     // 1) latch the start address for this burst
    //     adBus_set_address(base_addr);
    //     adBus_dir(false);  // release bus to the cartridge

    //     // 2) read 'words' sequential 16-bit values
    //     for (size_t i = 0; i < words; ++i) {
    //         uint16_t w = n64_read16();      // toggles /RD, waits, samples
    //         buf[2*i]   = (uint8_t)(w >> 8);   // MSB
    //         buf[2*i+1] = (uint8_t)(w & 0xFF); // LSB
    //     }

    //     // advance pointers
    //     base_addr += chunk;
    //     buf       += chunk;
    //     len       -= chunk;
    // }
    // return true;
}

/**
 * @brief Probe for battery-backed SRAM on the cartridge.
 *
 * Uses a float-bus precheck, then a read/write/restore pattern test.
 *
 * @return true if SRAM is detected and writable.
 */
bool gamepak_has_sram(void) {
    const uint32_t base = N64_GAMEPAK_SRAM_BASE;
    const uint32_t test_addr = base + 0x100;  // word-aligned safe offset
    const uint16_t magic = 0x5A5A;

    // 1) Float-bus check (no device = 0xFFFF)
    uint16_t first = gamepak_read_sram_word(base);
    if (first == 0xFFFF) {
        return false;
    }

    // 2) Read the original word at test_addr
    uint16_t orig = gamepak_read_sram_word(test_addr);

    // 3) Write our magic pattern
    if (!gamepak_write_sram_word(test_addr, magic)) {
        return false;
    }

    // 4) Read it back
    uint16_t readback = gamepak_read_sram_word(test_addr);

    // // 5) Restore the original
    gamepak_write_sram_word(test_addr, orig);

    // // 6) If we saw our pattern, SRAM is present
    return (readback == magic);
}


// #include <string.h> // For strlen

// bool gamepak_has_sram(void) {
//     const uint32_t base = N64_GAMEPAK_SRAM_BASE;
//     // We'll write starting at base + 0x100 to avoid critical areas near the beginning
//     const uint32_t write_start_addr = base + 0x100;
//     const char* test_string = "The quick brown fox jumps over the lazy dog."; // Your desired string

//     // 1) Float-bus check (no device = 0xFFFF)
//     uint16_t first = gamepak_read_sram_word(base);
//     if (first == 0xFFFF) {
//         return false;
//     }

//     // 2) (Original step 2 is no longer directly applicable as we are overwriting a block,
//     //    but we will still perform the write/read test to confirm SRAM presence).

//     // 3) Write our ASCII string to SRAM
//     // We need to write word by word (16 bits)
//     bool write_success = true;
//     for (int i = 0; i < strlen(test_string); i += 2) {
//         uint16_t word_to_write = 0;

//         // Pack two characters into one 16-bit word
//         // Assuming Big-Endian for N64, so first char is MSB, second is LSB
//         word_to_write |= (uint16_t)test_string[i] << 8;
//         if (i + 1 < strlen(test_string)) {
//             word_to_write |= (uint16_t)test_string[i+1];
//         } else {
//             // Pad with null if odd number of characters
//             word_to_write |= 0x00;
//         }

//         uint32_t current_addr = write_start_addr + (i / 2) * 2; // Each word is 2 bytes

//         if (!gamepak_write_sram_word(current_addr, word_to_write)) {
//             write_success = false;
//             break; // Exit on first write failure
//         }
//     }

//     if (!write_success) {
//         return false;
//     }

//     // 4) Read it back to verify
//     bool read_match = true;
//     for (int i = 0; i < strlen(test_string); i += 2) {
//         uint32_t current_addr = write_start_addr + (i / 2) * 2;
//         uint16_t readback_word = gamepak_read_sram_word(current_addr);

//         uint8_t char1 = (readback_word >> 8) & 0xFF; // MSB
//         uint8_t char2 = readback_word & 0xFF;        // LSB

//         if (char1 != test_string[i]) {
//             read_match = false;
//             break;
//         }
//         if (i + 1 < strlen(test_string) && char2 != test_string[i+1]) {
//             read_match = false;
//             break;
//         } else if (i + 1 >= strlen(test_string) && char2 != 0x00) {
//             // If it was the last char and it was odd, check padding
//             read_match = false;
//             break;
//         }
//     }

//     // 5) Restore the original - YOU EXPLICITLY WANT THIS COMMENTED OUT
//     //    gamepak_write_sram_word(test_addr, orig);

//     // 6) If we saw our pattern, SRAM is present
//     return read_match;
// }



/**
 * @brief Read a single 16-bit word from SRAM.
 *
 * @param addr SRAM address to read (word-aligned).
 * @return 16-bit SRAM data, or 0 if unavailable.
 */
uint16_t gamepak_read_sram_word(uint32_t addr) {
    // // 1) Drive address onto AD[0..15] and strobe ALE_H/ALE_L
    n64_adbus_set_address(addr);
    // // 2) Assert RD, sample data bus, release RD
    return n64_adbus_read16();
}


/**
 * @brief Read a 512-byte SRAM page into `buf`.
 * @param page_addr  Starting address of the page (must be word-aligned)
 * @param buf        Pointer to at least 512 bytes of storage
 * @return true on success
 */
bool gamepak_read_sram_page(uint32_t page_addr, uint8_t *buf) {
    if (!buf) return false;

    // SRAM is word-addressed, so we read 256 words = 512 bytes
    for (size_t w = 0; w < (N64_GAMEPAK_SRAM_PAGE_SIZE / 2); ++w) {
        uint32_t addr = page_addr + (uint32_t)(w * 2);
        uint16_t word = gamepak_read_sram_word(addr);
        buf[w*2    ] = (uint8_t)(word >> 8);
        buf[w*2 + 1] = (uint8_t)(word & 0xFF);
    }
    return true;
}


/**
 * @brief Write a single 16-bit word into GamePak SRAM.
 *
 * @param addr Word-aligned SRAM address.
 * @param data 16-bit value to write.
 * @return true on (likely) success.
 */
bool gamepak_write_sram_word(uint32_t addr, uint16_t data) {
    // 1) Drive bus to output
    n64_adbus_set_direction(true);

    // 2) Latch the address
    n64_adbus_set_address(addr);

    // 3) Write the word
    n64_adbus_write16(data);

    // 4) Return bus to read mode
    n64_adbus_set_direction(false);

    // TODO: If your hardware can detect write-ACK or /OE,
    //       you could verify here. For now assume it works.
    return true;
}

/**
 * @brief Dump the entire SRAM contents to stdout.
 *
 * @return true on success, false otherwise.
 */
#define N64_FAST_CHUNK_BYTES 1024u    // 1 KiB burst
#define N64_FAST_CHUNK_WORDS (N64_FAST_CHUNK_BYTES/2)

#define SRAM_SIZE_BYTES   (32 * 1024u)
#define SRAM_CHUNK_SIZE   256u   // bytes per chunk
static uint8_t sram_chunk_buffer[SRAM_CHUNK_SIZE];
bool gamepak_dump_sram(void) {
    // printf("Starting SRAM dump (HEX ONLY)...\n");
    // size_t bytes_on_line = 0;

    // for (uint32_t chunk_off = 0; chunk_off < SRAM_SIZE_BYTES; chunk_off += SRAM_CHUNK_SIZE) {
    //     // Read one 256-byte chunk (128 words)
    //     for (uint32_t word_off = 0; word_off < SRAM_CHUNK_SIZE; word_off += 2) {
    //         uint32_t addr = N64_ADDRESS_SRAM + chunk_off + word_off;
    //         uint16_t w   = sram_read_word(addr);
    //         // store big-endian
    //         sram_chunk_buffer[word_off    ] = (uint8_t)(w >> 8);
    //         sram_chunk_buffer[word_off + 1] = (uint8_t)(w & 0xFF);
    //     }
    //     // Print chunk as hex, 16 bytes per line
    //     for (uint32_t i = 0; i < SRAM_CHUNK_SIZE; ++i) {
    //         printf("%02X", sram_chunk_buffer[i]);
    //         if (++bytes_on_line >= 16) {
    //             // printf("\n");
    //             bytes_on_line = 0;
    //         }
    //     }
    // }
    // if (bytes_on_line) {
    //     printf("\n");
    // }
    // printf("\nSRAM dump complete.\n");
}