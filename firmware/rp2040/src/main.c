#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#include "n64/devices/gamepak.h"
#include "cli/core.h"
#include "utils/transport.h"


// Forward declaration for the N64 plugin registration
void plugin_n64_register(void);

/* ---------------- Helpers ------------------------------------------------ */
// This remains useful for a "Quick Info" command in your CLI
char g_info_file_buffer[512];

static void build_cart_info(void)
{
    const n64_gamepak_info_t *inf = gamepak_get_info();
    if (!inf) {
        strcpy(g_info_file_buffer, "--- GamePak Error! ---\n\nCartridge not detected.\n");
        return;
    }

    char title[21] = {0}; 
    gamepak_get_rom_title(title, sizeof title);
    
    char gid[5] = {0}; 
    memcpy(gid, inf->header.game_id, 4);

    char save[32];
    switch (inf->save_type) {
        case N64_SAVE_TYPE_SRAM:
            sprintf(save, "SRAM  (%u KB)", inf->save_size_bytes/1024); break;
        case N64_SAVE_TYPE_EEPROM_4K:  strcpy(save, "EEPROM 4 Kbit");  break;
        case N64_SAVE_TYPE_EEPROM_16K: strcpy(save, "EEPROM 16 Kbit"); break;
        case N64_SAVE_TYPE_FLASHRAM:
            sprintf(save, "FlashRAM (%u KB)", inf->save_size_bytes/1024); break;
        default: strcpy(save, "None"); break;
    }

    snprintf(g_info_file_buffer, sizeof g_info_file_buffer,
             "--- N64 Cartridge Info ---\n\n"
             "Title: %s\nID:    %s\nSave:  %s\n"
             "CRC1:  %08X\nCRC2:  %08X\n",
             title, gid, save, inf->header.crc1, inf->header.crc2);
}

static void n64_print_cart_summary(void) {
    const n64_gamepak_info_t *info = gamepak_get_info();
    if (!info || !info->valid) return;

    char title[32];
    gamepak_get_rom_title(title, sizeof(title));

    char id[5] = {0};
    memcpy(id, info->header.game_id, 4);

    const char *save_str = "None";
    switch (info->save_type) {
        case N64_SAVE_TYPE_SRAM:       save_str = "SRAM";       break;
        case N64_SAVE_TYPE_EEPROM_4K:  save_str = "EEPROM 4K";  break;
        case N64_SAVE_TYPE_EEPROM_16K: save_str = "EEPROM 16K"; break;
        case N64_SAVE_TYPE_FLASHRAM:   save_str = "FlashRAM";   break;
        default: break;
    }

    printf("\n  Cart : %s [%s]\n", title, id);
    printf("  Save : %s", save_str);
    if (info->save_size_bytes > 0)
        printf(" (%u bytes)", (unsigned)info->save_size_bytes);
    printf("\n");
    printf("  CRC  : %08X / %08X\n\n", info->header.crc1, info->header.crc2);
}


/* ----------------------------------------------------------------------- */
int main(void)
{
    // Initializes USB Serial (via SDK magic)
    stdio_init_all();
    transport_init();

    // Init N64 hardware pins and state
    gamepak_init();
    
    // Attempt an initial read of the cartridge
    build_cart_info();

    // Wait for the user to actually open a serial terminal (e.g. PuTTY, Screen, Minicom)
    // This prevents the "Welcome" message from being sent into the void.
    while (!stdio_usb_connected()) {
        sleep_ms(10);
    }
    sleep_ms(10);

    // Register our N64 functions into the CLI engine
    plugin_n64_register();
    
    // Setup the CLI state machine
    cli_core_init();
    n64_print_cart_summary();
    printf("Type 'help' for a list of commands.\n\n");
    
    while (true)
    {
        // Process incoming serial characters and run commands
        cli_core_task();
        // Low-power sleep to prevent the Pico from screaming at 100% CPU 
        // while waiting for human input.
        sleep_ms(1); 
    }
}