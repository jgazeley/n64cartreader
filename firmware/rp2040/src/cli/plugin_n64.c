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
#include "utils/packet.h"
#include "hardware/sync.h"

#include "n64/bus/adbus.h"
#include "n64/devices/gamepak.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

//------------------------------------------------------------------------------
// Forward Declarations (Private Command Handlers)
//------------------------------------------------------------------------------

// --- Implemented CLI Command Handlers ---
// static bool cmd_n64_init(const cli_args_t *args);
static bool cmd_n64_get_info(const cli_args_t *args);
static bool cmd_n64_hexdump_header(const cli_args_t *args);
static bool cmd_n64_hexdump_raw(const cli_args_t *args);
static bool cmd_n64_hexdump_save(const cli_args_t *args);
static bool cmd_n64_hexdump_eeprom(const cli_args_t *args);
static bool cmd_n64_hexdump_sram(const cli_args_t *args);
static bool cmd_n64_hexdump_fram(const cli_args_t *args);
static bool cmd_n64_write32_sram(const cli_args_t *args);
static bool cmd_n64_write32_eeprom(const cli_args_t *args);
static bool cmd_n64_write32_fram(const cli_args_t *args);
static bool cmd_n64_flash_diag(const cli_args_t *args);
static bool cmd_n64_repro_id(const cli_args_t *args);
static bool cmd_n64_export_save(const cli_args_t *args);
static bool cmd_n64_import_save(const cli_args_t *args);

static bool cmd_n64_dump_rom(const cli_args_t *args);
static bool cmd_n64_export_sram(const cli_args_t *args);
static bool cmd_n64_export_eeprom(const cli_args_t *args);
static bool cmd_n64_import_sram(const cli_args_t *args);
static bool cmd_n64_import_eeprom(const cli_args_t *args);
static bool cmd_n64_export_flashram(const cli_args_t *args);
static bool cmd_n64_import_flashram(const cli_args_t *args);

// --- Stubs for Future/Unimplemented Commands ---
static bool cmd_n64_verify_crc(const cli_args_t *args);
// static bool cmd_n64_shotgun(const cli_args_t *args);
// static bool n64_header_ascii(const cli_args_t *args); // Consider renaming to cmd_n64_view_header_ascii for consistency

//------------------------------------------------------------------------------
// Command Registration
//------------------------------------------------------------------------------

static void init_n64_commands(void) {
    static const cli_command_t n64_cmds[] = {
        // { "n64_cart_init",          cmd_n64_init,                     "Initialize the game cartridge"                   },
        { "n64_view_header",        cmd_n64_hexdump_header,           "View the first 64 bytes of the ROM in hex"       },
        { "n64_cart_info",          cmd_n64_get_info,                 "Initialize and display cartridge info"           },
        { "n64_dump_raw",           cmd_n64_hexdump_raw,              "View the save data in raw hex, no formatting"    },  
        { "n64_view_save",          cmd_n64_hexdump_save,             "View the save data (if it exists) in hex"        },       
        // { "n64_view_sram",          cmd_n64_hexdump_sram,             "View the SRAM (if it exists) in hex"             },
        // { "n64_view_eeprom",        cmd_n64_hexdump_eeprom,           "View the EEPROM (if it exists) in hex"           },
        // { "n64_view_fram",          cmd_n64_hexdump_fram,             "View the Flashram (if it exists) in hex"         },
        { "n64_write32_eeprom",     cmd_n64_write32_eeprom,           "Write & verify a 32-byte pattern to EEPROM"      },
        { "n64_write32_fram",       cmd_n64_write32_fram,             "Write & verify a 32-byte pattern to FRAM"        },
        { "n64_write32_sram",       cmd_n64_write32_sram,             "Write & verify a 32-byte pattern to SRAM"        },
        { "n64_dump_rom",           cmd_n64_dump_rom,                 "Dump full ROM to PC over serial"                 },
        { "n64_export_save",        cmd_n64_export_save,              "Export save to PC (auto-detect type)"            },
        { "n64_import_save",        cmd_n64_import_save,              "Import save from PC (auto-detect type)"          },
        // { "n64_export_sram",        cmd_n64_export_sram,              "Export full SRAM save to PC"                     },
        // { "n64_export_eeprom",      cmd_n64_export_eeprom,            "Export full EEPROM save to PC"                   },
        // { "n64_export_flashram",    cmd_n64_export_flashram,          "Export full Flashram save to PC"                   },
        // { "n64_import_sram",        cmd_n64_import_sram,              "Import save file from PC to SRAM"                },
        // { "n64_import_eeprom",      cmd_n64_import_eeprom,            "Import save file from PC to EEPROM"              },
        // { "n64_import_flashram",    cmd_n64_import_flashram,          "Import save file from PC to Flashram"            },
        { "n64_flash_diag",         cmd_n64_flash_diag,               "Raw FlashRAM bus diagnostic (no gamepak code)"   },
        { "n64_repro_id",           cmd_n64_repro_id,                 "Identify repro flash chip on ROM bus"            },
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
    { 'I', "ID:   Cart Information",                    "n64_cart_info",         NULL  },
    { 'H', "VIEW: Raw Header",                          "n64_view_header",      NULL  },
    { 'S', "VIEW: Save Data",                           "n64_view_save",        NULL  },
    // { 'E', "VIEW: EEPROM",                              "n64_view_eeprom",      NULL  },
    // { 'F', "VIEW: FRAM",                                "n64_view_fram",        NULL  },
    // { 'R', "VIEW: SRAM",                                "n64_view_sram",        NULL  },
    // { 'D', "DUMP: ROM to PC",                           "n64_dump_rom",         NULL  },
    // { 'O', "EXPORT: Save to PC",                        "n64_export_save",      NULL  },
    // { 'P', "IMPORT: Save from PC",                      "n64_import_save",      NULL  },
    // { 'G', "DIAG: Raw FlashRAM bus probe",              "n64_flash_diag",       NULL  },
    // { 'J', "TEST: Write 32B to EEPROM",                 "n64_write32_eeprom",   NULL  },
    // { 'W', "TEST: Write 32B to FRAM",                   "n64_write32_fram",     NULL  },
    // { 'M', "TEST: Write 32B to SRAM",                   "n64_write32_sram",     NULL  },  
    { 'X', "Back",                                       NULL,                  NULL  },
    // { 'X', "SPEW: Shotgun Bytes to stdout",      "shotgun",              NULL  },  
    // { 'E', "TEST: Erase FRAM",                   "n64_erase_fram",     NULL  },
    // { 'C', "TEST: Controller Test",              "controller_test",      NULL  },
    // { 'B', "TEST: Button Test",                   "button_test",          NULL  },
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

// static bool cmd_n64_init(const cli_args_t *args) {
//     (void)args;
//     printf("Initializing cartridge...\n");
//     if (!gamepak_init()) {
//         printf("ERROR: No cartridge detected.\n");
//         return true;
//     }
// //    build_cart_info();
//  //   printf("%s\n", g_info_file_buffer);
//     return true;
// }

static bool cmd_n64_verify_crc(const cli_args_t *args) {
    (void)args;
    printf("VERIFY CRC: not yet implemented\n");
    return true;
}

static bool cmd_n64_get_info(const cli_args_t *args) {
    (void)args;

    // // Check cartridge presence / init
    // if (!_gamepak_check_and_refresh()) {
    //     return true;
    // }

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
    
    // // First, call the gatekeeper. If it returns false, abort.
    // if (!_gamepak_check_and_refresh()) {
    //     return true; // Return true to prevent "Unknown command" error
    // }

    // If the check passed, we know the cached data is valid.
    const n64_gamepak_header_t* header = gamepak_get_header();

    printf("\n--- N64 ROM Header (64 Bytes) ---\n");
    utils_format_hexdump((const uint8_t*)header, N64_HEADER_SIZE, N64_ROM_BASE);
    
    return true;
}

static bool cmd_n64_hexdump_raw(const cli_args_t *args) {
    (void)args;
    
    // Ensure we have a fresh pull from the SRAM chip
    // _gamepak_refresh_save_page_cache();
    const uint8_t* buf = gamepak_get_save_page_buffer();
    
    printf("--- RAW DATA START ---\n");
    utils_format_hex_compact(buf, 512); // Dumps the first page
    printf("--- RAW DATA END ---\n");
    
    return true;
}

static bool cmd_n64_hexdump_sram(const cli_args_t *args) {
    (void)args;
    
    // // First, call the gatekeeper.
    // if (!_gamepak_check_and_refresh()) {
    //     return true;
    // }

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

    // // First, call the gatekeeper.
    // if (!_gamepak_check_and_refresh()) {
    //     return true;
    // }

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

    // // Gatekeeper: make sure we’ve probed and loaded the save page
    // if (!_gamepak_check_and_refresh()) {
    //     return true;
    // }

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

    // // Ensure the cartridge is ready.
    // if (!_gamepak_check_and_refresh()) {
    //     return true;
    // }

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

/**
 * @brief Writes a 64-byte test pattern to SRAM address 0x0000 and verifies it.
 */
static bool cmd_n64_write32_sram(const cli_args_t *args)
{
    (void)args;
    const size_t   test_size = 64;
    const uint32_t sram_addr = N64_SRAM_BASE;

    if (/*!_gamepak_check_and_refresh() ||*/
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

    if (/*!_gamepak_check_and_refresh() ||*/
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

static bool cmd_n64_write32_fram(const cli_args_t *args) {
    (void)args;
    uint8_t test_data[FLASHRAM_PAGE_SIZE]; // Must be full 128 bytes!
    memset(test_data, 0xFF, FLASHRAM_PAGE_SIZE); // Pad with FF
    
    // Put our 32-byte pattern at the start
    uint8_t pattern[32] = { 
        0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
        0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
        0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
        0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF
    };
    memcpy(test_data, pattern, 32);

    printf(">> Erasing FlashRAM Block 0...\n");
    if (!flashram_erase_block(0x08000000)) {
        printf("ERROR: Erase failed!\n");
        return false;
    }

    printf(">> Writing 128-byte test page to FlashRAM...\n");
    if (!flashram_program_page(0x08000000, test_data)) {
        printf("ERROR: FlashRAM program or verify failed.\n");
        return false;
    }
    
    printf("SUCCESS! Pattern written.\n");
    return true;
}

static bool cmd_n64_erase_fram(const cli_args_t *a)
{
    (void)a;

    if (/*!_gamepak_check_and_refresh() ||*/
        gamepak_get_save_type() != N64_SAVE_TYPE_FLASHRAM)
    {
        printf("INFO: FlashRAM not detected.\n");
        return true;
    }

    printf("\n>> Erasing FlashRAM (%u bytes = %u blocks)…\n",
           (unsigned)N64_FLASHRAM_SIZE,
           (unsigned)(N64_FLASHRAM_SIZE / FLASHRAM_BLOCK_SIZE));

    for (uint32_t addr = 0; addr < N64_FLASHRAM_SIZE; addr += FLASHRAM_BLOCK_SIZE)
    {
        if (!flashram_erase_block(addr))
        {
            printf("ERROR: erase failed @ 0x%06lX\n", (unsigned long)addr);
            return true;
        }
        //tud_task();                 /* keep USB CDC alive */
    }

    /* ---------- blank-check ---------- */
    static uint8_t page[FLASHRAM_PAGE_SIZE];
    for (uint32_t addr = 0; addr < N64_FLASHRAM_SIZE; addr += FLASHRAM_PAGE_SIZE)
    {
        if (!gamepak_read_flashram_bytes(addr, page, FLASHRAM_PAGE_SIZE))
        {
            printf("ERROR: readback failed @ 0x%06lX\n", (unsigned long)addr);
            return true;
        }
        for (size_t i = 0; i < FLASHRAM_PAGE_SIZE; ++i)
            if (page[i] != 0xFF)
            {
                printf("[FAILURE] erase verify mismatch @ 0x%06lX (%02X)\n",
                       (unsigned long)(addr + i), page[i]);
                return true;
            }
    }

    printf("[SUCCESS] device is blank (all 0xFF)\n");
    return true;
}


static bool cmd_n64_fullwrite_fram(const cli_args_t *a)
{
    (void)a;

    if (/*!_gamepak_check_and_refresh() ||*/
        gamepak_get_save_type() != N64_SAVE_TYPE_FLASHRAM)
    {
        printf("INFO: FlashRAM not detected.\n");
        return true;
    }

    printf("\n>> Writing 0xDEAD-0xBEEF pattern to entire FlashRAM…\n");

    /* Build one FLASHRAM_PAGE_SIZE-byte chunk of the pattern once */
    static uint8_t page[FLASHRAM_PAGE_SIZE];
    for (size_t i = 0; i < FLASHRAM_PAGE_SIZE / 2; ++i)
    {
        uint16_t w = (i & 1) ? 0xBEEF : 0xDEAD;
        page[2 * i]     = w >> 8;
        page[2 * i + 1] = (uint8_t)w;
    }

    /* Program a whole 128-KB sector at a time – requires a scratch buffer */
    static uint8_t *block = NULL;
    if (!block) block = malloc(FLASHRAM_BLOCK_SIZE);
    if (!block)
    {
        printf("ERROR: cannot allocate sector buffer\n");
        return true;
    }

    for (uint32_t base = 0; base < N64_FLASHRAM_SIZE; base += FLASHRAM_BLOCK_SIZE)
    {
        /* Replicate the pattern across the entire 128-KB block */
        for (uint32_t off = 0; off < FLASHRAM_BLOCK_SIZE; off += FLASHRAM_PAGE_SIZE)
            memcpy(block + off, page, FLASHRAM_PAGE_SIZE);

        if (!gamepak_write_flashram_sector(base, block))
        {
            printf("ERROR: program failed @ 0x%06lX\n", (unsigned long)base);
            return true;
        }
        //tud_task();                 /* service USB */
    }

    /* ---------- verify ---------- */
    for (uint32_t addr = 0; addr < N64_FLASHRAM_SIZE; addr += FLASHRAM_PAGE_SIZE)
    {
        if (!gamepak_read_flashram_bytes(addr, page, FLASHRAM_PAGE_SIZE))
        {
            printf("ERROR: readback failed @ 0x%06lX\n", (unsigned long)addr);
            return true;
        }

        /* Compare against golden pattern we built earlier */
        for (size_t i = 0; i < FLASHRAM_PAGE_SIZE; ++i)
        {
            uint8_t expect = ((i / 2) & 1) ? 0xEF : 0xDA;    /* BEEF or DEAD */
            if (page[i] != expect)
            {
                printf("[FAILURE] verify mismatch @ 0x%06lX (got %02X, want %02X)\n",
                       (unsigned long)(addr + i), page[i], expect);
                utils_format_hexdump(page, FLASHRAM_PAGE_SIZE, addr);
                return true;
            }
        }
    }

    printf("[SUCCESS] entire device verified OK – pattern intact\n");
    return true;
}

// Claude Code stuff
// ── Export ROM Dump ──────────────────────────────────────────────────────────────
static bool cmd_n64_dump_rom(const cli_args_t *args)
{
    (void)args;

    const n64_gamepak_info_t *info = gamepak_get_info();
    if (!info || !info->valid) {
        printf("ERROR: No cartridge detected. Run n64_init first.\n");
        return true;
    }

    uint32_t rom_size = info->rom_size_bytes;
    if (rom_size == 0) {
        printf("ERROR: ROM size unknown.\n");
        return true;
    }

    const uint16_t CHUNK_SIZE = 512;
    uint32_t num_chunks = rom_size / CHUNK_SIZE;
    uint8_t  chunk_buf[CHUNK_SIZE];
    uint8_t  seq = 0;

    // Tell Python the size, then signal ready to begin binary transfer
    printf("ROM_SIZE:%u\n", (unsigned)rom_size);
    printf("READY\n");

    for (uint32_t i = 0; i < num_chunks; i++) {
        uint32_t addr = N64_ROM_BASE + (i * CHUNK_SIZE);

        if (!gamepak_read_rom_bytes(addr, chunk_buf, CHUNK_SIZE)) {
            printf("ERROR: Read failed at chunk %u (0x%08X)\n", (unsigned)i, (unsigned)addr);
            return true;
        }

        if (!packet_send_reliable(chunk_buf, CHUNK_SIZE, seq)) {
            printf("ERROR: Transfer failed at chunk %u\n", (unsigned)i);
            return true;
        }

        seq++; // wraps naturally at 255 -> 0, Python matches this
    }
    sleep_ms(10); 
    printf("DUMP_COMPLETE\n");
    return true;
}

// ── Export Save (auto-detect) ────────────────────────────────────────────────
static bool cmd_n64_export_save(const cli_args_t *args)
{
    switch (gamepak_get_save_type()) {
        case N64_SAVE_TYPE_SRAM:
            return cmd_n64_export_sram(args);
        case N64_SAVE_TYPE_EEPROM_4K:
        case N64_SAVE_TYPE_EEPROM_16K:
            return cmd_n64_export_eeprom(args);
        case N64_SAVE_TYPE_FLASHRAM:
            return cmd_n64_export_flashram(args);
        default:
            printf("No save chip detected.\n");
            return true;
    }
}

// ── Import Save (auto-detect) ────────────────────────────────────────────────
static bool cmd_n64_import_save(const cli_args_t *args)
{
    switch (gamepak_get_save_type()) {
        case N64_SAVE_TYPE_SRAM:
            return cmd_n64_import_sram(args);
        case N64_SAVE_TYPE_EEPROM_4K:
        case N64_SAVE_TYPE_EEPROM_16K:
            return cmd_n64_import_eeprom(args);
        case N64_SAVE_TYPE_FLASHRAM:
            return cmd_n64_import_flashram(args);
        default:
            printf("No save chip detected.\n");
            return true;
    }
}

// ── Export SRAM ──────────────────────────────────────────────────────────────
static bool cmd_n64_export_sram(const cli_args_t *args)
{
    (void)args;
    if (gamepak_get_save_type() != N64_SAVE_TYPE_SRAM) {
        printf("ERROR: SRAM not detected.\n");
        return true;
    }

    const uint16_t CHUNK_SIZE = 512;
    const uint32_t num_chunks = N64_SRAM_SIZE / CHUNK_SIZE;
    static uint8_t chunk_buf[512];
    uint8_t seq = 0;

    printf("SAVE_TYPE:SRAM\n");
    printf("SAVE_SIZE:%u\n", (unsigned)N64_SRAM_SIZE);
    printf("READY\n");

    for (uint32_t i = 0; i < num_chunks; i++) {
        uint32_t addr = N64_SRAM_BASE + (i * CHUNK_SIZE);
        if (!gamepak_read_sram_bytes(addr, chunk_buf, CHUNK_SIZE)) {
            printf("ERROR: SRAM read failed at chunk %u\n", (unsigned)i);
            return true;
        }
        if (!packet_send_reliable(chunk_buf, CHUNK_SIZE, seq)) {
            printf("ERROR: Transfer failed at chunk %u\n", (unsigned)i);
            return true;
        }
        seq++;
    }
    sleep_ms(10);
    printf("EXPORT_COMPLETE\n");
    return true;
}

// ── Export EEPROM ─────────────────────────────────────────────────────────────
static bool cmd_n64_export_eeprom(const cli_args_t *args)
{
    (void)args;
    n64_save_type_t st = gamepak_get_save_type();
    if (st != N64_SAVE_TYPE_EEPROM_4K && st != N64_SAVE_TYPE_EEPROM_16K) {
        printf("ERROR: EEPROM not detected.\n");
        return true;
    }

    size_t   save_size  = gamepak_get_save_size();
    const uint16_t CHUNK_SIZE = 512;
    uint32_t num_chunks = save_size / CHUNK_SIZE;
    if (num_chunks == 0) num_chunks = 1;  // 4K = 512 bytes = 1 chunk
    static uint8_t chunk_buf[512];
    uint8_t seq = 0;

    printf("SAVE_TYPE:EEPROM\n");
    printf("SAVE_SIZE:%u\n", (unsigned)save_size);
    printf("READY\n");

    for (uint32_t i = 0; i < num_chunks; i++) {
        uint32_t addr = i * CHUNK_SIZE;
        size_t   this_chunk = (save_size - addr < CHUNK_SIZE)
                              ? (save_size - addr) : CHUNK_SIZE;
        if (!gamepak_read_eeprom_bytes(addr, chunk_buf, this_chunk)) {
            printf("ERROR: EEPROM read failed at chunk %u\n", (unsigned)i);
            return true;
        }
        if (!packet_send_reliable(chunk_buf, (uint16_t)this_chunk, seq)) {
            printf("ERROR: Transfer failed at chunk %u\n", (unsigned)i);
            return true;
        }
        seq++;
    }
    sleep_ms(10);
    printf("EXPORT_COMPLETE\n");
    return true;
}

// ── Import SRAM ───────────────────────────────────────────────────────────────
static bool cmd_n64_import_sram(const cli_args_t *args)
{
    (void)args;
    if (gamepak_get_save_type() != N64_SAVE_TYPE_SRAM) {
        printf("ERROR: SRAM not detected.\n");
        return true;
    }

    const uint16_t CHUNK_SIZE = 512;
    const uint32_t num_chunks = N64_SRAM_SIZE / CHUNK_SIZE;
    static uint8_t chunk_buf[512];
    uint8_t seq = 0;

    printf("SAVE_SIZE:%u\n", (unsigned)N64_SRAM_SIZE);
    printf("READY\n");

    for (uint32_t i = 0; i < num_chunks; i++) {
        if (!packet_receive_reliable(chunk_buf, CHUNK_SIZE, seq)) {
            printf("ERROR: Receive failed at chunk %u\n", (unsigned)i);
            return true;
        }
        uint32_t addr = N64_SRAM_BASE + (i * CHUNK_SIZE);
        if (!gamepak_write_sram_bytes(addr, chunk_buf, CHUNK_SIZE)) {
            printf("ERROR: SRAM write failed at chunk %u\n", (unsigned)i);
            return true;
        }
        seq++;
    }
    sleep_ms(10);
    printf("IMPORT_COMPLETE\n");
    return true;
}

// ── Import EEPROM ─────────────────────────────────────────────────────────────
static bool cmd_n64_import_eeprom(const cli_args_t *args)
{
    (void)args;
    n64_save_type_t st = gamepak_get_save_type();
    if (st != N64_SAVE_TYPE_EEPROM_4K && st != N64_SAVE_TYPE_EEPROM_16K) {
        printf("ERROR: EEPROM not detected.\n");
        return true;
    }

    size_t   save_size  = gamepak_get_save_size();
    const uint16_t CHUNK_SIZE = 512;
    uint32_t num_chunks = save_size / CHUNK_SIZE;
    if (num_chunks == 0) num_chunks = 1;
    static uint8_t chunk_buf[512];
    uint8_t seq = 0;

    printf("SAVE_SIZE:%u\n", (unsigned)save_size);
    printf("READY\n");

    for (uint32_t i = 0; i < num_chunks; i++) {
        uint32_t addr = i * CHUNK_SIZE;
        size_t   this_chunk = (save_size - addr < CHUNK_SIZE)
                              ? (save_size - addr) : CHUNK_SIZE;
        if (!packet_receive_reliable(chunk_buf, (uint16_t)this_chunk, seq)) {
            printf("ERROR: Receive failed at chunk %u\n", (unsigned)i);
            return true;
        }
        if (!gamepak_write_and_verify_eeprom_bytes(addr, chunk_buf, this_chunk)) {
            printf("ERROR: EEPROM write failed at chunk %u\n", (unsigned)i);
            return true;
        }
        seq++;
    }
    sleep_ms(10);
    printf("IMPORT_COMPLETE\n");
    return true;
}

// ── Import FlashRAM ──────────────────────────────────────────────────────────
static bool cmd_n64_import_flashram(const cli_args_t *args)
{
    (void)args;
    if (gamepak_get_save_type() != N64_SAVE_TYPE_FLASHRAM) {
        printf("ERROR: FlashRAM not detected.\n");
        return true;
    }

    const uint16_t CHUNK_SIZE = 512;
    const uint16_t PAGE_SIZE  = 128;
    const uint32_t num_chunks = N64_FLASHRAM_SIZE / CHUNK_SIZE;
    static uint8_t chunk_buf[512];
    uint8_t seq = 0;

    printf("SAVE_SIZE:%u\n", (unsigned)N64_FLASHRAM_SIZE);
    
    // Step 1: Erase THE ENTIRE CHIP (All 8 banks)
    printf("ERASING ALL BANKS...\n");
    // Ensure your flashram_erase_block function loops 8 times internally
    if (!flashram_erase_block(0x08000000)) { 
        printf("ERROR: Erase failed.\n");
        return true;
    }

    printf("READY\n");

    for (uint32_t i = 0; i < num_chunks; i++) {
        // Clear buffer before receiving
        while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT); 
        
        if (!packet_receive_reliable(chunk_buf, CHUNK_SIZE, seq)) {
            printf("\nERROR: Receive failed at chunk %u\n", (unsigned)i);
            return true;
        }

        uint32_t base_addr = i * CHUNK_SIZE;

        // Step 2: Write the 512-byte chunk (4 pages)
        for (int p = 0; p < 4; p++) {
            if (!flashram_program_page(base_addr + (p * PAGE_SIZE), &chunk_buf[p * PAGE_SIZE])) {
                printf("\nERROR: Program failed at chunk %u, page %d\n", (unsigned)i, p);
                return true;
            }
        }

        // Step 3: Manual Throttle 
        // We MUST let the Python script know we are done with the physical write.
        // If your packet_receive_reliable ACKs automatically, Python is already 
        // sending the next chunk. 
        // Add a small delay here to ensure the Pico is ready for the next getchar.
        sleep_ms(5); 

        // If your packet_receive_reliable DOES NOT ACK, uncomment the next line:
        // putchar(0x06); fflush(stdout);

        seq++;
    }
    sleep_ms(10);
    printf("IMPORT_COMPLETE\n");
    return true;
}

// ── Export FlashRAM ──────────────────────────────────────────────────────────
static bool cmd_n64_export_flashram(const cli_args_t *args)
{
    (void)args;
    if (gamepak_get_save_type() != N64_SAVE_TYPE_FLASHRAM) {
        printf("ERROR: FlashRAM not detected.\n");
        return true;
    }

    const uint16_t CHUNK_SIZE = 512;
    const uint32_t num_chunks = N64_FLASHRAM_SIZE / CHUNK_SIZE;
    static uint8_t chunk_buf[512];
    uint8_t seq = 0;

    printf("SAVE_TYPE:FLASHRAM\n");
    printf("SAVE_SIZE:%u\n", (unsigned)N64_FLASHRAM_SIZE);
    printf("READY\n");

    for (uint32_t i = 0; i < num_chunks; i++) {
        uint32_t addr = i * CHUNK_SIZE;
        if (!gamepak_read_flashram_bytes(addr, chunk_buf, CHUNK_SIZE)) {
            printf("ERROR: FlashRAM read failed at chunk %u\n", (unsigned)i);
            return true;
        }
        if (!packet_send_reliable(chunk_buf, CHUNK_SIZE, seq)) {
            printf("ERROR: Transfer failed at chunk %u\n", (unsigned)i);
            return true;
        }
        seq++;
    }

    sleep_ms(10);
    printf("EXPORT_COMPLETE\n");
    return true;
}


// ── Raw FlashRAM bus diagnostic ────────────────────────────────────────────
// Talks directly to the bus. Does NOT call any gamepak_*flashram* functions.
static void diag_flash_cmd(uint32_t cmd) {
    uint16_t low  = (uint16_t)(cmd & 0xFFFFu);
    uint16_t high = (uint16_t)(cmd >> 16);
    adbus_set_direction(true);
    adbus_latch_address(0x08010000u);
    adbus_write_word(high);
    adbus_write_word(low);
    adbus_set_direction(false);
}

static void diag_flash_read8(uint32_t addr, uint8_t out[8]) {
    adbus_latch_address(addr);
    for (int i = 0; i < 8; i += 2) {
        uint16_t w = adbus_read_word();
        out[i]     = (uint8_t)(w >> 8);
        out[i + 1] = (uint8_t)(w & 0xFF);
    }
}

static void diag_print8(const char *label, const uint8_t b[8]) {
    printf("  %-20s: %02X %02X %02X %02X %02X %02X %02X %02X\n",
           label, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
}

static bool cmd_n64_flash_diag(const cli_args_t *args)
{
    (void)args;
    uint8_t buf[8];

    printf("\n=== FlashRAM Bus Diagnostic ===\n");
    printf("Reading raw bus at 0x08000000 with no commands first...\n");

    diag_flash_read8(0x08000000u, buf);
    diag_print8("cold read", buf);

    printf("\nStep 1: RESET cmd (0xFF000000)\n");
    diag_flash_cmd(0xFF000000u);
    sleep_ms(10);
    diag_flash_read8(0x08000000u, buf);
    diag_print8("after reset", buf);

    printf("\nStep 2: Second RESET + 50ms wait\n");
    diag_flash_cmd(0xFF000000u);
    sleep_ms(50);
    diag_flash_read8(0x08000000u, buf);
    diag_print8("after 2nd reset", buf);

    printf("\nStep 3: READ_ARRAY cmd (0xF0000000)\n");
    diag_flash_cmd(0xF0000000u);
    sleep_ms(10);
    diag_flash_read8(0x08000000u, buf);
    diag_print8("after read_array", buf);

    printf("\nStep 4: STATUS_MODE cmd (0xE1000000)\n");
    diag_flash_cmd(0xE1000000u);
    sleep_us(200);
    diag_flash_read8(0x08000000u, buf);
    diag_print8("status/ID", buf);

    printf("\nStep 5: Final RESET\n");
    diag_flash_cmd(0xFF000000u);
    sleep_ms(10);
    diag_flash_read8(0x08000000u, buf);
    diag_print8("final state", buf);

    printf("\nStep 6: Aggressive — RESET to data addr 0x08000000\n");
    adbus_set_direction(true);
    adbus_latch_address(0x08000000u);
    adbus_write_word(0x00FF);
    adbus_write_word(0x0000);
    adbus_set_direction(false);
    sleep_ms(10);

    diag_flash_cmd(0xE1000000u);
    sleep_us(200);
    diag_flash_read8(0x08000000u, buf);
    diag_print8("after aggressive", buf);

    diag_flash_cmd(0xFF000000u);

    printf("\n=== Done ===\n");
    printf("Good = 11 11 80 01 00 C2 00 1E (Macronix)\n");
    printf("       11 11 80 01 00 32 00 F1 (Panasonic)\n");
    printf("Stuck= 00 8C repeating\n");

    return true;
}


// ── Repro Flash chip identification ────────────────────────────────────────
// Sends the standard CFI 0xAA/0x55/0x90 ID sequence on the ROM bus.
// Works with Macronix, Spansion, ST, Intel, Fujitsu repro flash chips.
static bool cmd_n64_repro_id(const cli_args_t *args)
{
    (void)args;
    const uint32_t base = N64_ROM_BASE;
    uint16_t v, d;

    printf("\n=== Repro Flash ID ===\n");

    // Confirm bus is alive
    adbus_latch_address(base);
    uint16_t h = adbus_read_word();
    printf("  Header word  : 0x%04X\n", h);
    if (h != 0x8037 && h != 0x4012 && h != 0x3780) {
        printf("  Bus bad. Power cycle needed.\n");
        return true;
    }

    // ── Protocol 1: Standard CFI (single-width) ──
    adbus_set_direction(true);
    adbus_latch_address(base + (0x555 << 1));  adbus_write_word(0x00AA);
    adbus_latch_address(base + (0x2AA << 1));  adbus_write_word(0x0055);
    adbus_latch_address(base + (0x555 << 1));  adbus_write_word(0x0090);
    adbus_set_direction(false);
    sleep_us(200);
    adbus_latch_address(base);
    v = adbus_read_word();  d = adbus_read_word();
    printf("  Standard CFI : V=0x%04X D=0x%04X\n", v, d);
    // Reset
    adbus_set_direction(true);
    adbus_latch_address(base);  adbus_write_word(0x00F0);
    adbus_set_direction(false);
    sleep_ms(50);
    // Verify
    adbus_latch_address(base);
    printf("    recovered  : 0x%04X\n", adbus_read_word());

    // ── Protocol 2: Doubled (Fujitsu/dual-chip) ──
    adbus_set_direction(true);
    adbus_latch_address(base + (0x555 << 1));  adbus_write_word(0xAAAA);
    adbus_latch_address(base + (0x2AA << 1));  adbus_write_word(0x5555);
    adbus_latch_address(base + (0x555 << 1));  adbus_write_word(0x9090);
    adbus_set_direction(false);
    sleep_us(200);
    adbus_latch_address(base);
    v = adbus_read_word();  d = adbus_read_word();
    printf("  Doubled CFI  : V=0x%04X D=0x%04X\n", v, d);
    // Reset
    adbus_set_direction(true);
    adbus_latch_address(base);  adbus_write_word(0xF0F0);
    adbus_set_direction(false);
    sleep_ms(50);
    adbus_latch_address(base);
    printf("    recovered  : 0x%04X\n", adbus_read_word());

    // ── Protocol 3: Intel (0x90 direct) ──
    adbus_set_direction(true);
    adbus_latch_address(base);  adbus_write_word(0x0090);
    adbus_set_direction(false);
    sleep_us(200);
    adbus_latch_address(base);
    v = adbus_read_word();  d = adbus_read_word();
    printf("  Intel        : V=0x%04X D=0x%04X\n", v, d);
    // Reset
    adbus_set_direction(true);
    adbus_latch_address(base);  adbus_write_word(0x00FF);
    adbus_set_direction(false);
    sleep_ms(50);
    adbus_latch_address(base);
    printf("    recovered  : 0x%04X\n", adbus_read_word());

    // ── Protocol 4: Samsung (0x90 to 0x000) ──
    adbus_set_direction(true);
    adbus_latch_address(base + 0x000);  adbus_write_word(0x0090);
    adbus_set_direction(false);
    sleep_us(200);
    adbus_latch_address(base);
    v = adbus_read_word();
    adbus_latch_address(base + 0x02);
    d = adbus_read_word();
    printf("  Samsung      : V=0x%04X D=0x%04X\n", v, d);
    // Reset
    adbus_set_direction(true);
    adbus_latch_address(base);  adbus_write_word(0x00F0);
    adbus_set_direction(false);
    sleep_ms(50);
    adbus_latch_address(base);
    printf("    recovered  : 0x%04X\n", adbus_read_word());

    printf("=== Done ===\n");
    return true;
}