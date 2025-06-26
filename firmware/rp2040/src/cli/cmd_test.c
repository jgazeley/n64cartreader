//cmd_test.c
#include "cli/command.h"
#include "cli/cmd_test.h"
#include "utils/format.h"
#include "utils/packet.h"
#include "utils/byteswap.h"
#include "n64/devices/gamepak.h"
#include "n64/bus/adbus.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TEST_BUF_SIZE 256

// A simple test buffer in RAM
static uint8_t s_test_buffer[TEST_BUF_SIZE];
static uint8_t s_test_buffer2[N64_GAMEPAK_HEADER_SIZE];

// Forward declarations of the handlers
static bool cmd_fill_buffer(const cli_args_t *args);
static bool cmd_dump_buffer(const cli_args_t *args);
static bool cmd_dump_buffer_swapped(const cli_args_t *args);
static bool cmd_write_message(const cli_args_t *args);
static bool cmd_stream_buffer(const cli_args_t *args);
static bool cmd_send_buffer(const cli_args_t *args);
// static bool cmd_cart_info(const cli_args_t *args);
// static bool cmd_dump_rom(const cli_args_t *args);
static bool cmd_debug_show_header(const cli_args_t *args);
static bool cmd_debug_show_info(const cli_args_t *args);

// Pointers to the registered descriptors (for menu lookup)
const cli_command_t *cmd_fill_buffer_desc;
const cli_command_t *cmd_dump_buffer_desc;
const cli_command_t *cmd_write_message_desc;
const cli_command_t *cmd_stream_buffer_desc;
const cli_command_t *cmd_send_buffer_desc;

// Called from core_init to install these test commands
void test_cmds_init(void) {
static const cli_command_t test_cmds[] = {
        { "fillbuf",   cmd_fill_buffer,   "Fill buffer with incremental bytes" },
        { "dumpbuf",   cmd_dump_buffer,   "Hex-dump the test buffer"           },
        { "dumpbuf-s", cmd_dump_buffer_swapped, "Hex-dump (swapped 16-bit)"    },
        { "writemsg",  cmd_write_message, "Write a fixed message into buffer"  },
        { "streambuf", cmd_stream_buffer, "Raw-stream the test buffer (no CRC)"},
        { "sendbuf",   cmd_send_buffer,   "Send the test buffer as a framed packet" },
        { "n64_header", cmd_debug_show_header, "Dump the first 64 bytes" },
        { "n64_info",   cmd_debug_show_info,  "Initialize and display cartridge info" },
    };
    cli_command_register(test_cmds,
                         sizeof test_cmds / sizeof *test_cmds);

    // look up all descriptor pointers for menu.c
    cli_command_lookup("fillbuf",   &cmd_fill_buffer_desc);
    cli_command_lookup("dumpbuf",   &cmd_dump_buffer_desc);
    cli_command_lookup("writemsg",  &cmd_write_message_desc);
    cli_command_lookup("streambuf", &cmd_stream_buffer_desc);
    cli_command_lookup("sendbuf",   &cmd_send_buffer_desc);
}

/**
 * @brief CLI command to dump the first 256 bytes of the GamePak ROM.
 */
// static bool cmd_dump_rom(const cli_args_t *args) {
//     (void)args; // Suppress unused parameter warning

//     if (!gamepak_is_valid()) {
//         printf("Cartridge not initialized. Please run 'cart-info' first.\n");
//         return true;
//     }

//     uint8_t rom_buffer[256];
//     printf("Reading first 256 bytes from ROM base (0x%08lX)...\n", (unsigned long)N64_GAMEPAK_ROM_BASE);

//     if (!gamepak_read_rom(N64_GAMEPAK_ROM_BASE, rom_buffer, sizeof(rom_buffer))) {
//         printf("Error: Failed to read from ROM.\n");
//         return true;
//     }

//     printf("Read successful. Hex dump of data:\n");

//     // Use the hex dump utility to print the buffer
//     hex_dump_options_t opts = {
//         .bytes_per_line = 16,
//         .show_ascii     = true,
//     };
//     utils_format_hexdump_ex(rom_buffer, sizeof(rom_buffer), N64_GAMEPAK_ROM_BASE, &opts);
    
//     return true;
// }

/**
 * @brief CLI command to initialize and display GamePak info.
 */
// static bool cmd_cart_info(const cli_args_t *args) {
//     (void)args; // Suppress unused parameter warning

//     printf("Initializing GamePak...\n");
//     if (!gamepak_init()) {
//         printf("Error: Failed to initialize GamePak. Is a cartridge inserted?\n");
//         return true;
//     }

//     const gamepak_info_t* info = gamepak_get_info();
//     if (!info) {
//         printf("No valid cartridge info found after init.\n");
//         return true;
//     }

//     printf("\n--- N64 GamePak Info ---\n");
//     printf("Game Title:   %s\n", gamepak_header_title());
//     printf("Country Code: 0x%02X\n", gamepak_header_country());
//     printf("Version:      %d\n", gamepak_header_version());
//     printf("CRC1 Checksum:  0x%08lX\n", gamepak_header_crc1());
//     printf("CRC2 Checksum:  0x%08lX\n", gamepak_header_crc2());

//     printf("Save Type:    ");
//     switch(gamepak_save_type()) {
//         case GAMEPAK_SAVE_NONE:       printf("None Detected\n"); break;
//         case GAMEPAK_SAVE_EEPROM_4K:  printf("EEPROM 4Kbit\n"); break;
//         case GAMEPAK_SAVE_EEPROM_16K: printf("EEPROM 16Kbit\n"); break;
//         case GAMEPAK_SAVE_SRAM:       printf("SRAM 256Kbit\n"); break;
//         case GAMEPAK_SAVE_FLASHRAM:   printf("FlashRAM 1Mbit\n"); break;
//         default:                      printf("Unknown\n"); break;
//     }
//     printf("------------------------\n");

//     return true;
// }


// Handler: fill buffer with 0x00,0x01,0x02, … pattern
static bool cmd_fill_buffer(const cli_args_t *args) {
    (void)args;
    for (size_t i = 0; i < TEST_BUF_SIZE; i++) {
        s_test_buffer[i] = (uint8_t)i;
    }
    printf("Test buffer filled (0…%u)\n", TEST_BUF_SIZE - 1);
    return true;
}

// Handler: hex-dump the entire buffer
static bool cmd_dump_buffer(const cli_args_t *args) {
    (void)args;
    hexdump_options_t opts = {
        .bytes_per_line = 16,
        .max_rows       = 0,    // unlimited
        .show_ascii     = true
    };
    utils_format_hexdump_ex(s_test_buffer,
                             TEST_BUF_SIZE,
                             0x00000000,  // base address label
                             &opts);
    return true;
}

static bool cmd_dump_buffer_swapped(const cli_args_t *args)
{
    // Suppress unused parameter warning, just like in your example
    (void)args;

    // A simple test buffer of 16-bit values
    uint16_t test_data[] = { 0x1234, 0x5678, 0x9ABC, 0xDEF0, 0xFF00, 0x00FF };
    size_t num_elements = sizeof(test_data) / sizeof(test_data[0]);

    printf("--- Original Buffer ---\n");
    utils_format_hexdump(test_data, sizeof(test_data), 0);
    
    // Swap each 16-bit element in place
    for (size_t i = 0; i < num_elements; i++) {
        test_data[i] = byteswap16(test_data[i]);
    }

    printf("\n--- Buffer after 16-bit Swap ---\n");
    utils_format_hexdump(test_data, sizeof(test_data), 0);

    // Return true to indicate the command executed successfully
    return true;
}

// Handler: write a fixed string into buffer start
static bool cmd_write_message(const cli_args_t *args) {
    (void)args;
    const char *msg = "Hello, CLI World!";
    size_t len = strlen(msg);
    if (len >= TEST_BUF_SIZE) len = TEST_BUF_SIZE - 1;
    memcpy(s_test_buffer, msg, len);
    s_test_buffer[len] = 0;
    printf("Wrote message: \"%s\"\n", msg);
    return true;
}

// Handler: raw-stream 256 bytes (no framing or CRC)
static bool cmd_stream_buffer(const cli_args_t *args) {
    (void)args;
    printf(">> Streaming %u bytes raw…\n", TEST_BUF_SIZE);
    if (!packet_stream(s_test_buffer, TEST_BUF_SIZE)) {
        printf("!! packet_stream failed\n");
        return false;
    }
    printf("<< Done\n");
    return true;
}

// Handler: framed packet of 256 bytes (sync+len+CRC)
static bool cmd_send_buffer(const cli_args_t *args) {
    (void)args;
    printf(">> Sending %u-byte framed packet…\n", TEST_BUF_SIZE);
    if (!packet_send(s_test_buffer, TEST_BUF_SIZE)) {
        printf("!! packet_send failed\n");
        return false;
    }
    printf("<< Packet sent\n");
    return true;
}

static bool cmd_debug_show_header(const cli_args_t *args) {
    (void)args; // Not used

    uint8_t header[N64_GAMEPAK_HEADER_SIZE];

    // Option 1: Use your abstracted function (preferred)
    // bool ok = gamepak_read_bytes(N64_GAMEPAK_ROM_BASE, header, N64_GAMEPAK_HEADER_SIZE);

    // Option 2: If you want direct, raw bus access, swap to this:
    adbus_init(); // Only if not already done elsewhere
    for (size_t i = 0; i < N64_GAMEPAK_HEADER_SIZE; i += 2) {
        adbus_latch_address(N64_GAMEPAK_ROM_BASE + i);
        uint16_t w = adbus_read_word();
        header[i]   = (uint8_t)(w >> 8);
        header[i+1] = (uint8_t)(w & 0xFF);
    }
    bool ok = true;

    if (!ok) {
        printf("Failed to read header at ROM base address 0x%08X!\n", N64_GAMEPAK_ROM_BASE);
        return false;
    }

    printf("\n--- N64 ROM Header @ 0x%08X ---\n", N64_GAMEPAK_ROM_BASE);
    utils_format_hexdump(header, N64_GAMEPAK_HEADER_SIZE, N64_GAMEPAK_ROM_BASE);
    printf("-------------------------------\n");

    return true;
}

static bool cmd_debug_show_info(const cli_args_t *args) {

}