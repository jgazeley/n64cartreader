// ─────────────────────────────────────────────────────────────────────────────
// File: src/cli/plugin_n64.c
// Description: N64 plugin for dumping ROM header and cartridge info.
// ─────────────────────────────────────────────────────────────────────────────

#include "cli/command.h"
#include "cli/menu.h"
#include "cli/plugins.h"
#include "cli/plugin_n64.h"
#include "cli/io.h"
#include "utils/format.h"

#include "n64/devices/gamepak.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

//------------------------------------------------------------------------------
// Forward Declarations (Private Command Handlers)
//------------------------------------------------------------------------------
// --- Private Helper Functions ---
static bool _gamepak_check_and_refresh(void);

// --- Implemented CLI Command Handlers ---
static bool cmd_n64_get_info(const cli_args_t *args);
static bool cmd_n64_hexdump_header(const cli_args_t *args);
static bool cmd_n64_hexdump_save(const cli_args_t *args);
static bool cmd_n64_hexdump_eeprom(const cli_args_t *args);
static bool cmd_n64_hexdump_sram(const cli_args_t *args);
static bool cmd_n64_hexdump_fram(const cli_args_t *args);
static bool cmd_n64_write32_sram(const cli_args_t *args);
static bool cmd_n64_write32_fram(const cli_args_t *args);

// --- Stubs for Future/Unimplemented Commands ---
static bool cmd_n64_shotgun(const cli_args_t *args);
static bool cmd_n64_verify_crc(const cli_args_t *args);
static bool cmd_n64_write32_eeprom(const cli_args_t *args);
static bool n64_header_ascii(const cli_args_t *args); // Consider renaming to cmd_n64_view_header_ascii for consistency

//------------------------------------------------------------------------------
// Command Registration
//------------------------------------------------------------------------------

static void init_n64_commands(void) {
    static const cli_command_t n64_cmds[] = {
        { "n64_dump_header",  cmd_n64_hexdump_header,           "View the first 64 bytes of the ROM in hex"    },
        { "n64_get_info",     cmd_n64_get_info,                 "Initialize and display cartridge info"        },
        { "n64_dump_save",    cmd_n64_hexdump_save,             "View the save data (if it exists) in hex"          },       
        // { "n64_dump_sram",    cmd_n64_hexdump_sram,             "View the SRAM (if it exists) in hex"          },
        // { "n64_dump_eeprom",  cmd_n64_hexdump_eeprom,           "View the EEPROM (if it exists) in hex"        },
        // { "n64_dump_fram",    cmd_n64_hexdump_fram,             "View the Flashram (if it exists) in hex"        },
        // { "n64_write32_sram", cmd_n64_write32_sram,             "Write & verify a 32-byte pattern to SRAM"     },
        { "n64_write32_fram", cmd_n64_write32_fram,             "Write & verify a 32-byte pattern to FRAM"     },
        // { "controller_test",  cmd_n64_test_controller,          "Probe the Joybus for a controller"            },
        // { "button_test",      cmd_n64_test_buttons,             "Read raw data from the controller"            },
        // { "shotgun",      cmd_n64_shotgun,              "Spew data from the ROM (header over and over))"   },
        // { "header_ascii", n64_header_ascii,             "Test.. "},
    };
    cli_command_register(n64_cmds,
                         sizeof(n64_cmds) / sizeof(*n64_cmds));
}

//------------------------------------------------------------------------------
// Menu Definition
//------------------------------------------------------------------------------

static const menu_item_t n64_menu_items[] = {
    { 'I', "ID:   Cart Information",           "n64_get_info",         NULL  },
    { 'H', "VIEW: Raw Header",                 "n64_dump_header",      NULL  },
    { 'S', "VIEW: Save Data",                       "n64_dump_save",        NULL  },
    // { 'S', "VIEW: SRAM",                       "n64_dump_sram",        NULL  },
    // { 'E', "VIEW: EEPROM",                     "n64_dump_eeprom",      NULL  },
    // { 'F', "VIEW: FRAM",                       "n64_dump_fram",        NULL  },
    // { 'X', "SPEW: Shotgun Bytes to stdout",    "shotgun",              NULL  },
    // { 'U', "DUMP: SRAM (to stdout)",           "debug_dump_sram",      NULL  },
    // { 'P', "DUMP: EEPROM (to stdout)",         "debug_dump_eeprom",    NULL  },
    { 'W', "TEST: Write 32B to FRAM",          "n64_write32_fram",     NULL  },
    // { 'C', "TEST: Controller Test",            "controller_test",      NULL  },
    // { 'B', "TEST: Button Test",                "button_test",          NULL  },
    // { 'X', "TEST: Write 64B to EEPROM",        "debug_write32_eeprom", NULL  },
    { 'X', "Back",                             NULL,                   NULL  },
};

const menu_frame_t n64_menu_frame = {
    .title = "N64 Options",
    .items = n64_menu_items,
    .count = sizeof(n64_menu_items) / sizeof(*n64_menu_items),
};

const menu_frame_t * const n64_menu = &n64_menu_frame;

//------------------------------------------------------------------------------
// Public Registration Function
//------------------------------------------------------------------------------
void plugin_n64_register(void) {
    cli_register_command_init(init_n64_commands);
    // Menu registration is handled by the main application via n64_menu
}

//------------------------------------------------------------------------------
// Command Handlers
//------------------------------------------------------------------------------

static bool cmd_n64_verify_crc(const cli_args_t *args) {
    (void)args;
    printf("VERIFY CRC: not yet implemented\n");
    return true;
}

static bool _gamepak_check_and_refresh(void) {
    // // 1. First, check if a cart was ever initialized successfully at all.
    // // gamepak_get_info() returns NULL if the initial gamepak_init() failed.
    // if (gamepak_get_info() == NULL) {
    //     printf("ERROR: GamePak not initialized. Please insert a cartridge and reboot.\n");
    //     return false;
    // }

    // // 2. If a cart WAS initialized, do a live check to see if it's still there.
    // // gamepak_is_present() compares the live header to the one from init.
    // if (!gamepak_is_present()) {
    //     // If this check fails, the cart was changed or removed.
    //     printf("\n--- Cartridge Changed or Removed ---\n");
    //     printf("Re-scanning cartridge slot... ");
        
    //     // Re-run init silently to reset the driver state and find a new cart if present.
    //     if (gamepak_init()) {
    //         printf("New cartridge detected and initialized.\n");
    //     } else {
    //         printf("No valid cartridge detected.\n");
    //     }
        
    //     // Instruct the user to try again, since the context has changed.
    //     printf("Please try your command again.\n");
    //     return false; // Tell the calling function to abort its operation.
    // }

    // // If we get here, the original cart is still present and valid.
    return true; // Tell the calling function it's safe to proceed.
}

static bool cmd_n64_get_info(const cli_args_t *args) {
    (void)args;

    // Check cartridge presence / init
    if (!_gamepak_check_and_refresh()) {
        return true;
    }

    const n64_gamepak_info_t *info = gamepak_get_info();
    const n64_gamepak_header_t *h = &info->header;

    char title_buf[32];
    gamepak_get_rom_title(title_buf, sizeof(title_buf));

    // Build 4-char Game ID buffer
    char id_buf[5] = {0};
    memcpy(id_buf, h->game_id, 4);

    printf("\n>> Cartridge Information:\n");
    printf("  Title           : \"%s\"\n", title_buf);
    printf("  Game ID         : %s\n", id_buf);
    printf("  Version         : 0x%02X\n", h->version);
    printf("  Initial Settings: 0x%08X\n", h->initial_settings);
    printf("  Clock Rate      : 0x%08X\n", h->clock_rate);
    printf("  Entry Point PC  : 0x%08X\n", h->pc_start);
    printf("  Release Address : 0x%08X\n", h->release_addr);
    printf("  CRC1            : 0x%08X\n", h->crc1);
    printf("  CRC2            : 0x%08X\n", h->crc2);

    // Save information
    printf("  Save Type       : ");
    switch (info->save_type) {
        case N64_SAVE_TYPE_SRAM:
            printf("SRAM (%zu bytes)\n", info->save_size_bytes);
            break;
        case N64_SAVE_TYPE_EEPROM_4K:
            printf("EEPROM 4Kbit (%zu bytes)\n", info->save_size_bytes);
            break;
        case N64_SAVE_TYPE_EEPROM_16K:
            printf("EEPROM 16Kbit (%zu bytes)\n", info->save_size_bytes);
            break;
        case N64_SAVE_TYPE_FLASHRAM:
            printf("FlashRAM (%zu bytes)\n", info->save_size_bytes);
            break;
        case N64_SAVE_TYPE_NONE:
            printf("None\n");
            break;
        default:
            printf("Unknown (%zu bytes)\n", info->save_size_bytes);
            break;
    }

    printf("\n");
    return true;
}

static bool cmd_n64_hexdump_header(const cli_args_t *args) {
    (void)args;
    
    // First, call the gatekeeper. If it returns false, abort.
    if (!_gamepak_check_and_refresh()) {
        return true; // Return true to prevent "Unknown command" error
    }

    // If the check passed, we know the cached data is valid.
    const n64_gamepak_header_t* header = gamepak_get_header();

    printf("\n--- N64 ROM Header (64 Bytes) ---\n");
    utils_format_hexdump((const uint8_t*)header, N64_HEADER_SIZE, N64_ROM_BASE);
    
    return true;
}

static bool cmd_n64_hexdump_sram(const cli_args_t *args) {
    (void)args;
    
    // First, call the gatekeeper.
    if (!_gamepak_check_and_refresh()) {
        return true;
    }

    if (gamepak_get_save_type() != N64_SAVE_TYPE_SRAM) {
        printf("INFO: SRAM not detected on this cartridge.\n");
        return true;
    }
    
    const uint8_t* sram_buffer = gamepak_get_save_page_buffer();

    printf("\n--- N64 SRAM (First 512 bytes) ---\n");
    utils_format_hexdump(sram_buffer, N64_SAVE_PAGE_BUFFER_SIZE, N64_SRAM_BASE);
    
    return true;
}

static bool cmd_n64_hexdump_eeprom(const cli_args_t *args) {
    (void)args;

    // First, call the gatekeeper.
    if (!_gamepak_check_and_refresh()) {
        return true;
    }

    n64_save_type_t save_type = gamepak_get_save_type();
    if (save_type != N64_SAVE_TYPE_EEPROM_4K && save_type != N64_SAVE_TYPE_EEPROM_16K) {
        printf("INFO: EEPROM not detected on this cartridge.\n");
        return true;
    }
    
    const uint8_t* eeprom_buffer = gamepak_get_save_page_buffer();
    size_t display_size = (save_type == N64_SAVE_TYPE_EEPROM_4K) ? N64_EEPROM_4K_SIZE : N64_SAVE_PAGE_BUFFER_SIZE;
    
    printf("\n--- N64 EEPROM Data (first %u bytes) ---\n", display_size);
    utils_format_hexdump(eeprom_buffer, display_size, 0);
    
    return true;
}

static bool cmd_n64_hexdump_fram(const cli_args_t *args) {
    (void)args;

    // Gatekeeper: make sure we’ve probed and loaded the save page
    if (!_gamepak_check_and_refresh()) {
        return true;
    }

    // Only proceed if the detected save type is FlashRAM
    if (gamepak_get_save_type() != N64_SAVE_TYPE_FLASHRAM) {
        printf("INFO: FlashRAM not detected on this cartridge.\n");
        return true;
    }

    // Get pointer to the 512-byte save-page buffer
    const uint8_t* flashram_buffer = gamepak_get_save_page_buffer();

    printf("\n--- N64 FlashRAM (first 512 bytes) ---\n");
    utils_format_hexdump(
        flashram_buffer,
        N64_SAVE_PAGE_BUFFER_SIZE,  // 64-byte buffer × 8 = 512 bytes
        N64_SRAM_BASE                   // base address where FlashRAM is mapped
    );

    return true;
}

static bool cmd_n64_hexdump_save(const cli_args_t *args) {
    // This command takes no arguments, but we pass the pointer along
    // to the underlying functions that require it in their signature.
    (void)args;

    // Ensure the cartridge is ready.
    if (!_gamepak_check_and_refresh()) {
        return true;
    }

    // Get the detected save type.
    n64_save_type_t save_type = gamepak_get_save_type();

    // Call the appropriate, dedicated command handler.
    switch (save_type) {
        case N64_SAVE_TYPE_SRAM:
            return cmd_n64_hexdump_sram(args);

        case N64_SAVE_TYPE_EEPROM_4K:
        case N64_SAVE_TYPE_EEPROM_16K:
            return cmd_n64_hexdump_eeprom(args);

        case N64_SAVE_TYPE_FLASHRAM:
            // Assuming you create a dedicated cmd_n64_hexdump_fram function
            return cmd_n64_hexdump_fram(args);

        case N64_SAVE_TYPE_NONE:
        default:
            printf("No save chip detected on this cartridge.\n");
            break;
    }

    return true;
}

// /**
//  * @brief Writes a 64-byte test pattern to the start of SRAM and verifies it.
//  */
// static bool cmd_n64_write32_sram(const cli_args_t *args) {
//     (void)args;

//     const uint32_t test_address = N64_SRAM_BASE;
//     const size_t test_size = 64;

//     // Gatekeeper check for a valid, SRAM-enabled cartridge.
//     if (!_gamepak_check_and_refresh() || gamepak_get_save_type() != N64_SAVE_TYPE_SRAM) {
//         printf("INFO: SRAM not detected on this cartridge. Cannot perform write test.\n");
//         return true;
//     }

//     // --- 1. Prepare the test pattern IN A BUFFER ---
//     uint8_t write_buffer[test_size];
//     for (size_t i = 0; i < (test_size / 2); i++) {
//         // Alternate between 0xDEAD and 0xBEEF for each 16-bit word
//         uint16_t word_to_write = (i % 2 == 0) ? 0xDEAD : 0xBEEF;
        
//         // Place the two bytes of the word into the buffer
//         write_buffer[i * 2]     = (uint8_t)(word_to_write >> 8); // High byte
//         write_buffer[i * 2 + 1] = (uint8_t)(word_to_write & 0xFF); // Low byte
//     }
    
//     printf("\n>> Preparing to write %zu-byte test pattern to SRAM...\n", test_size);
//     printf("Data to be written:\n");
//     // Now we pass the buffer (which is a pointer) to the hexdump function.
//     utils_format_hexdump(write_buffer, test_size, test_address);

//     // --- 2. Write the data using the GamePak API ---
//     printf("\n>> Writing data...\n");
//     // Pass the buffer to the write function.
//     if (!gamepak_write_sram_bytes(test_address, write_buffer, test_size)) {
//         printf("ERROR: API call to gamepak_write_sram_bytes() failed.\n");
//         return true;
//     }
//     printf("Write operation completed.\n");

//     // --- 3. Read the data back for verification ---
//     printf("\n>> Reading data back for verification...\n");
//     uint8_t read_buffer[test_size];
//     if (!gamepak_read_sram_bytes(test_address, read_buffer, test_size)) {
//         printf("ERROR: API call to gamepak_read_sram_bytes() failed during verification.\n");
//         return true;
//     }
//     printf("Read-back complete. Verifying...\n");
    
//     // --- 4. Compare and report the result ---
//     // Now the memcmp can correctly compare the two buffers.
//     if (memcmp(write_buffer, read_buffer, test_size) == 0) {
//         printf("\n[SUCCESS]: Data verified correctly!\n");
//         printf("The contents of SRAM at 0x%08X now are:\n", test_address);
//         utils_format_hexdump(read_buffer, test_size, test_address);
//     } else {
//         printf("\n[FAILURE]: Read-back data does not match what was written!\n");
//         printf("Expected data:\n");
//         utils_format_hexdump(write_buffer, test_size, test_address);
//         printf("Actual data read from SRAM:\n");
//         utils_format_hexdump(read_buffer, test_size, test_address);
//     }

//     return true;
// }

/**
 * @brief Writes a 64-byte test pattern to SRAM address 0x0000 and verifies it.
 */
static bool cmd_n64_write32_sram(const cli_args_t *args)
{
    (void)args;
    const size_t   test_size = 64;
    const uint32_t sram_addr = N64_SRAM_BASE;

    if (!_gamepak_check_and_refresh() ||
        gamepak_get_save_type() != N64_SAVE_TYPE_SRAM)
    {
        printf("INFO: SRAM not detected on this cartridge.\n");
        return true;
    }

    /* Build the 0xDEAD/0xBEEF pattern ----------------------------------- */
    uint8_t wr[test_size];
    for (size_t i = 0; i < test_size/2; ++i) {
        uint16_t w = (i & 1) ? 0xBEEF : 0xDEAD;
        wr[2*i]   = w >> 8;
        wr[2*i+1] = (uint8_t)w;
    }

    printf("\n>> Writing 64-byte test pattern to SRAM...\n");
    utils_format_hexdump(wr, test_size, sram_addr);

    /* Write ---------------------------------------------------------------- */
    if (!gamepak_write_sram_bytes(sram_addr, wr, test_size)) {
        printf("ERROR: SRAM write failed.\n");
        return true;
    }

    /* Read back & verify --------------------------------------------------- */
    uint8_t rd[test_size];
    if (!gamepak_read_sram_bytes(sram_addr, rd, test_size)) {
        printf("ERROR: SRAM read-back failed.\n");
        return true;
    }

    if (memcmp(wr, rd, test_size) == 0) {
        printf("[SUCCESS] verified OK:\n");
        utils_format_hexdump(rd, test_size, sram_addr);
    } else {
        printf("[FAILURE] data mismatch!\n");
    }
    return true;
}

static bool cmd_n64_write32_eeprom(const cli_args_t *args)
{
    (void)args;
    const size_t   test_size = 64;
    const uint32_t eep_addr  = 0;

    if (!_gamepak_check_and_refresh() ||
        (gamepak_get_save_type() != N64_SAVE_TYPE_EEPROM_4K &&
         gamepak_get_save_type() != N64_SAVE_TYPE_EEPROM_16K))
    {
        printf("INFO: EEPROM not detected on this cartridge.\n");
        return true;
    }

    uint8_t write_buf[test_size];
    for (size_t i = 0; i < (test_size/2); ++i)
    {
        uint16_t w = (i & 1) ? 0xBEEF : 0xDEAD;
        write_buf[2*i]   = (uint8_t)(w >> 8);
        write_buf[2*i+1] = (uint8_t) w;
    }

    printf("\n>> Writing 64-byte test pattern to EEPROM...\n");
    utils_format_hexdump(write_buf, test_size, eep_addr);

    if (!gamepak_write_and_verify_eeprom_bytes(eep_addr, write_buf, test_size))
    {
        printf("ERROR: EEPROM write or verify failed.\n");
        return true;
    }

    uint8_t read_buf[test_size];
    gamepak_read_eeprom_bytes(eep_addr, read_buf, test_size);

    if (memcmp(write_buf, read_buf, test_size) == 0)
    {
        printf("[SUCCESS] verified OK:\n");
        utils_format_hexdump(read_buf, test_size, eep_addr);
    }
    else
    {
        printf("[FAILURE] data mismatch!\n");
    }
    return true;
}

static bool cmd_n64_write32_fram(const cli_args_t *a)
{
    (void)a;
    const size_t test_size   = 64;
    const uint32_t test_addr = 0;          /* first 64 bytes */

    if (!_gamepak_check_and_refresh() ||
        gamepak_get_save_type() != N64_SAVE_TYPE_FLASHRAM)
    {
        printf("INFO: FlashRAM not detected.\n");
        return true;
    }

    uint8_t wr[test_size];
    for (size_t i = 0; i < test_size/2; ++i) {
        uint16_t w = (i & 1) ? 0xBEEF : 0xDEAD;
        wr[2*i]   = w >> 8;
        wr[2*i+1] = (uint8_t)w;
    }

    printf("\n>> Writing 64-byte test pattern to FlashRAM...\n");
    utils_format_hexdump(wr, test_size, test_addr);

    if (!gamepak_write_flashram_bytes(test_addr, wr, test_size)) {
        printf("ERROR: FlashRAM write or verify failed.\n");
        return true;
    }

    uint8_t rd[test_size];
    gamepak_read_flashram_bytes(test_addr, rd, test_size);

    if (memcmp(wr, rd, test_size) == 0) {
        printf("[SUCCESS] verified OK:\n");
        utils_format_hexdump(rd, test_size, test_addr);
    } else {
        printf("[FAILURE] data mismatch!\n");
    }
    return true;
}






















///// V V V    Hacky shit for testing       V V V




// const char* joybus_peripheral_to_string(n64_joybus_peripheral_t peripheral) {
//     switch (peripheral) {
//         case JOYBUS_PERIPHERAL_NONE:
//             return "No Device Detected";
        
//         case JOYBUS_PERIPHERAL_CONTROLLER:
//             return "Standard N64 Controller";
            
//         case JOYBUS_PERIPHERAL_EEPROM_4K:
//             return "On-Cart EEPROM (4Kbit)";
            
//         case JOYBUS_PERIPHERAL_EEPROM_16K:
//             return "On-Cart EEPROM (16Kbit)";
            
//         case JOYBUS_PERIPHERAL_UNKNOWN:
//             return "Unknown Device";
            
//         default:
//             return "Invalid/Unspecified Peripheral";
//     }
// }




// static bool cmd_n64_test_controller(const cli_args_t *args) {
//     (void)args;
    
//     if (joybus_identify_peripheral() != JOYBUS_PERIPHERAL_CONTROLLER) {
//         printf("ERROR: Standard controller not detected.\n");
//         return true;
//     }
    
//     printf("SUCCESS: Controller detected. Starting live poll...\n");
//     printf("Press 'q' on your keyboard to quit.\n\n");

//     n64_controller_state_t controller_state; // The struct to hold the data

//     while (true) {
//         if (cli_io_read_char_nonblocking() == 'q') {
//             break;
//         }

//         // The API call fills our struct with data
//         if (joybus_read_controller_state(&controller_state)) {
//             // We pass a POINTER to the struct to our print helper
//             _print_controller_state(&controller_state);
//         } else {
//             printf("\rState: TIMEOUT - Controller disconnected.                  ");
//             fflush(stdout);
//         }

//         sleep_ms(50);
//     }

//     printf("\n\n--- Live poll finished. ---\n");
//     return true;
// }


static bool cmd_n64_test_controller(const cli_args_t *args) {
    // (void)args;

    // printf("\n>> Probing Joybus for peripheral...\n");

    // // --- This block now performs the raw probe directly ---
    // // We need the private helper functions from joybus.c for this test.
    // bool _joybus_read_bytes(uint8_t* buffer, size_t len);
    // void _joybus_send_command(const uint8_t* command, size_t len);

    // uint8_t info_command[] = {0xFF}; // The standard "Info" command
    // uint8_t response[3];

    // // Send the command and attempt to read the 3-byte response.
    // _joybus_send_command(info_command, sizeof(info_command));
    // if (!_joybus_read_bytes(response, sizeof(response))) {
    //     printf("ERROR: Timeout while waiting for response. No device detected.\n");
    //     return true;
    // }

    // // --- This is the new, critical debugging output ---
    // uint32_t device_id = ((uint32_t)response[0] << 16) | ((uint32_t)response[1] << 8) | response[2];
    // printf("INFO: Received Raw Device ID: 0x%02X%02X%02X\n", response[0], response[1], response[2]);


    // // --- Now, we interpret the raw ID ---
    // if (device_id == 0x050002) {
    //     printf("INFO: ID matches 'Standard Controller'.\n");
    // } else if (device_id == 0x050001) {
    //     printf("INFO: ID matches 'Controller with Pak'.\n");
    // } else if (device_id == 0x008001) {
    //     printf("INFO: ID matches 'EEPROM 4Kbit'.\n");
    // } else if (device_id == 0x00C001) {
    //     printf("INFO: ID matches 'EEPROM 16Kbit'.\n");
    // } else {
    //     printf("INFO: Device ID is unknown.\n");
    // }
    
    // // --- Finally, proceed only if it's a controller ---
    // if (device_id == 0x050002 || device_id == 0x050001) {
    //     printf("\nController detected. Starting live poll...\n");
    //     printf("Press 'q' on your keyboard to quit.\n\n");

    //     n64_controller_state_t controller_state;
    //     while (true) {
    //         if (cli_io_read_char_nonblocking() == 'q') {
    //             break;
    //         }

    //         if (joybus_read_controller_state(&controller_state)) {
    //             _print_controller_state(&controller_state);
    //         } else {
    //             printf("\rState: TIMEOUT - Controller disconnected.                  ");
    //             fflush(stdout);
    //         }
    //         sleep_ms(50);
    //     }
    //     printf("\n\n--- Live poll finished. ---\n");
    // }

    // return true;
}

// In src/cli/plugin_n64.c, with the other cmd_ functions
static bool cmd_n64_test_buttons(const cli_args_t *args) {
    // (void)args;

    // // We need to use the global PIO variables from joybus.c for this hack
    // extern PIO pio;
    // extern uint piooffset;
    // extern pio_sm_config config;
    
    // printf("\n--- Starting Live Controller Poll ---\n");
    // printf("Press buttons on the N64 controller and watch the values change.\n");
    // printf("Press 'q' on your keyboard to quit.\n\n");

    // // Loop forever until the user quits
    // while (true) {
    //     // Check if the user wants to quit
    //     int key = cli_io_read_char_nonblocking();
    //     if (key == 'q' || key == 'Q') {
    //         break;
    //     }

    //     // --- Send the "Poll" command (0x01) ---
    //     uint8_t poll_command[] = {0x01};
    //     uint32_t pio_command_buffer[8];
    //     int pio_command_len;
    //     convertToPio(poll_command, 1, pio_command_buffer, &pio_command_len);
        
    //     // Send the command using the same logic as the identify test
    //     pio_sm_set_enabled(pio, 0, false);
    //     pio_sm_init(pio, 0, piooffset + joybus_offset_outmode, &config);
    //     pio_sm_set_enabled(pio, 0, true);
    //     for (int i = 0; i < pio_command_len; i++) {
    //         pio_sm_put_blocking(pio, 0, pio_command_buffer[i]);
    //     }

    //     // --- Read the 4-byte response ---
    //     uint32_t byte1 = GetInputWithTimeout();
    //     uint32_t byte2 = GetInputWithTimeout();
    //     uint32_t byte3 = GetInputWithTimeout();
    //     uint32_t byte4 = GetInputWithTimeout();

    //     // Print the raw hex values to the screen on a single line
    //     // The \r (carriage return) makes the line overwrite itself
    //     printf("\rController State: 0x%02X 0x%02X 0x%02X 0x%02X", byte1, byte2, byte3, byte4);
        
    //     // Force the output buffer to flush so we see the update immediately
    //     fflush(stdout);

    //     // A short delay to prevent spamming the bus too hard
    //     sleep_ms(50);
    // }

    // printf("\n\n--- Live poll finished. ---\n");
    return true;
}

// static void _print_controller_state(const n64_controller_state_t* state) {
//     // // The \r carriage return moves the cursor to the start of the line.
//     // printf("\rState: ");

//     // // Now we access the clean boolean fields from the struct.
//     // if (state->a) printf("A ");
//     // if (state->b) printf("B ");
//     // if (state->z) printf("Z ");
//     // if (state->start) printf("S ");
//     // if (state->d_up) printf("DU ");
//     // if (state->d_down) printf("DD ");
//     // if (state->d_left) printf("DL ");
//     // if (state->d_right) printf("DR ");
//     // if (state->l) printf("L ");
//     // if (state->r) printf("R ");
//     // if (state->c_up) printf("CU ");
//     // if (state->c_down) printf("CD ");
//     // if (state->c_left) printf("CL ");
//     // if (state->c_right) printf("CR ");

//     // // Access the joystick axes from the struct.
//     // printf("  Joy X/Y: %4d,%4d", state->x_axis, state->y_axis);
    
//     // // Print spaces to clear the rest of the line from previous, longer prints.
//     // printf("                                ");

//     // // Force the output buffer to flush so the line updates immediately.
//     // fflush(stdout);
// }

static uint8_t _joybus_read_byte_decoded(void) {
    // // extern PIO pio; // HACK: Access the global PIO handle

    // uint8_t decoded_byte = 0;
    // for (int i = 0; i < 8; i++) {
    //     // Read the timing value from the PIO FIFO. Add a timeout for safety.
    //     uint32_t pulse_width_raw = pio_sm_get_with_timeout_us(pio, 0, 500);

    //     // The PIO pushes a 5-bit representation of how many loops it did.
    //     // A short pulse ('1' bit) will run out the counter, leaving a small number.
    //     // A long pulse ('0' bit) will exit early, leaving a large number.
    //     // This threshold value distinguishes between the two.
    //     bool bit = (pulse_width_raw < 15);

    //     // Shift the decoded bit into our byte
    //     decoded_byte = (decoded_byte << 1) | bit;
    // }
    // return decoded_byte;
}