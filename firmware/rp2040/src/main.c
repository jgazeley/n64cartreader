/**
 * @file main.c
 * @brief Main entry point. Initializes the system once on boot.
 */
// #include "pico/stdlib.h"
// #include "tusb.h"
// #include <stdio.h>
// #include "n64/devices/gamepak.h"

// #ifdef ENABLE_CLI
//   #include "cli/core.h"
//   #include "cli/menu.h" // For menu_render()
//   void plugin_n64_register(void);
// #else
//   #include "utils/packet.h"
//   #include "utils/transport.h"
// #endif

// static bool stream_gamepak(uint32_t base, size_t total);
// static bool stream_flashram(size_t total_size);

// int main(void) {
//     // 1. Common Initialization for both modes
//     tusb_init();
//     stdio_usb_init();

//     // 2. Wait for USB serial connection to be established
//     while (!tud_cdc_connected()) {
//         tud_task();
//         sleep_ms(10);
//     }

//     #ifndef ENABLE_CLI
//         transport_init();
//     #endif

//     // 3. Initialize the GamePak hardware
//     // printf("\n--- System Boot. Initializing GamePak... ---\n");
//     if (!gamepak_init()) {
//         // printf("ERROR: GamePak failed to initialize. Check cartridge.\n");
//         // printf("System halted.\n");
//         // while(1) { tight_loop_contents(); } // Halt on failure
//     }
//     // printf("SUCCESS: GamePak initialized.\n");


//     // 4. Conditional setup based on the build mode
// #ifdef ENABLE_CLI
//     // printf("Mode: Full CLI Application\n\n");
//     plugin_n64_register();
//     cli_core_init();
// #else
//     // printf("Mode: Streamer-Only. Send 'G' to dump ROM.\n\n");
// #endif


//     // The main application loop
//     while (1) {
//         // tud_task() is always required for USB
//         tud_task();

//     #ifdef ENABLE_CLI
//         // --- CLI Mode Logic ---
//         cli_core_task();
//     #else
// //         // --- Streamer-Only Mode Logic ---
// //         if (tud_cdc_available()) {
// //             uint8_t cmd;
// //             if (tud_cdc_read(&cmd, 1) == 1 && (cmd == 'G' || cmd == 'g')) {
// //                 printf("Stream command 'G' received. Starting ROM dump...\n");
// //                 // Dump the first 1MB of the ROM as an example
// //                 bool success = stream_gamepak(N64_ROM_BASE, 8 * 1024);
// //                 if (success) {
// //                     printf("Dump complete.\n");
// //                 } else {
// //                     printf("Dump failed.\n");
// //                 }
// //             }
// //         }
// //     #endif
// //     }
// // }
//         // --- Streamer-Only Mode Logic ---
//         if (tud_cdc_available()) {
//             uint8_t cmd;
//             if (tud_cdc_read(&cmd, 1) == 1) {
//                 // 'G' command to dump the GamePak ROM
//                 if (cmd == 'G' || cmd == 'g') {
//                     // printf("Stream command 'G' received. Starting ROM dump...\n");
//                     // Dump the first 1MB of the ROM as an example
//                     bool success = stream_gamepak(N64_ROM_BASE, 512);
//                     if (success) {
//                         // printf("Dump complete.\n");
//                     } else {
//                         // printf("Dump failed.\n");
//                     }
//                 }
//                 // NEW: 'F' command to dump the FlashRAM save
//                 else if (cmd == 'F' || cmd == 'f') {
//                     // printf("Stream command 'F' received. Starting FlashRAM dump...\n");
//                     bool success = stream_flashram(N64_FLASHRAM_SIZE);
//                     if (success) {
//                     //     printf("Dump complete.\n");
//                     } else {
//                         // printf("Dump failed.\n");
//                     }
//                 }
//             }
//         }
//     #endif
//     }
// }

// #if !defined(ENABLE_CLI)

// static bool stream_flashram(size_t total_size) {
//     // This buffer is used for each chunk. 1024 bytes is a good size.
//     static uint8_t buf[1024];
//     size_t sent = 0;

//     // printf("Streaming %zu bytes from FlashRAM...\n", total_size);

//     // Loop until the total amount has been sent
//     while (sent < total_size) {
//         // Determine the size of the next chunk to process
//         size_t chunk_size = sizeof(buf);
//         if (sent + chunk_size > total_size) {
//             chunk_size = total_size - sent;
//         }

//         // Read one chunk from the correct offset in FlashRAM
//         if (!gamepak_read_flashram_bytes(sent, buf, chunk_size)) {
//             // printf("ERROR: Failed to read from FlashRAM at offset 0x%zX.\n", sent);
//             return false;
//         }

//         // Stream the chunk that was just read
//         if (tud_cdc_write(buf, chunk_size) < chunk_size) { // <--- REPLACEMENT
//             // Not all bytes were written to the buffer, handle error
//             // printf("ERROR: Failed to write all bytes to CDC buffer.\n");
//             return false;
//         }

//         // Flush the USB buffer to ensure the host receives the data promptly
//         tud_cdc_write_flush();
//         sent += chunk_size;
//     }

//     return true;
// }


// static bool stream_gamepak(uint32_t base, size_t total) {
//     // Note: PACKET_WRITE_CHUNK_BYTES must be defined, e.g., in packet.h
//     static uint8_t buf[PACKET_WRITE_CHUNK_BYTES];
//     size_t sent = 0;

//     while (sent < total) {
//         size_t chunk = PACKET_WRITE_CHUNK_BYTES;
//         if (sent + chunk > total) {
//             chunk = total - sent;
//         }

//         // Use the clean GamePak API to read the ROM data
//         if (!gamepak_read_rom_bytes(base + sent, buf, chunk)) {
//             printf("ERROR: Failed to read from GamePak at address 0x%08X\n", base + sent);
//             return false;
//         }

//         // Use the packet streamer utility to send the data
//         if (!packet_stream(buf, chunk)) {
//             printf("ERROR: Failed to stream packet.\n");
//             return false;
//         }

//         tud_cdc_write_flush();
//         sent += chunk;
//     }
//     return true;
// }
// #endif





// /*
//  *  main.c  – pico-pak entry point
//  *  Works in two build variants:
//  *     • ENABLE_CLI defined  → composite USB (CDC+MSC) + CLI console
//  *     • ENABLE_CLI undefined→ mass-storage only
//  */

// #include "pico/stdlib.h"
// #include "tusb.h"
// #include <stdio.h>
// #include <string.h>

// #include "n64/devices/gamepak.h"

// #ifdef ENABLE_CLI
//   #include "cli/core.h"
//   void plugin_n64_register(void);
// #else
//   #include "usb/filesystem.h"           /* MSC only */
// #endif

// /* ---------------- CART_INFO.TXT (always needed) ------------------------- */
// char g_info_file_buffer[512];

// /* ---------------- Helpers ------------------------------------------------ */
// static void build_cart_info(void)
// {
//   const n64_gamepak_info_t *inf = gamepak_get_info();
//   if (!inf) {
//     strcpy(g_info_file_buffer,
//            "--- GamePak Error! ---\r\n\r\nCartridge not detected.\r\n");
//     return;
//   }

//   char title[21] = {0}; gamepak_get_rom_title(title, sizeof title);
//   char gid[5]    = {0}; memcpy(gid, inf->header.game_id, 4);

//   char save[32];
//   switch (inf->save_type) {
//     case N64_SAVE_TYPE_SRAM:
//       sprintf(save, "SRAM  (%u KB)", inf->save_size_bytes/1024); break;
//     case N64_SAVE_TYPE_EEPROM_4K:  strcpy(save, "EEPROM 4 Kbit");  break;
//     case N64_SAVE_TYPE_EEPROM_16K: strcpy(save, "EEPROM 16 Kbit"); break;
//     case N64_SAVE_TYPE_FLASHRAM:
//       sprintf(save, "FlashRAM (%u KB)", inf->save_size_bytes/1024); break;
//     default: strcpy(save, "None"); break;
//   }

//   snprintf(g_info_file_buffer, sizeof g_info_file_buffer,
//            "--- N64 Cartridge Info ---\r\n\r\n"
//            "Title: %s\r\nID:    %s\r\nSave:  %s\r\n"
//            "CRC1:  %08X\r\nCRC2:  %08X\r\n",
//            title, gid, save, inf->header.crc1, inf->header.crc2);
// }

// /* ----------------------------------------------------------------------- */
// int main(void)
// {
//   stdio_init_all();

//   /* -------- N64 cartridge / JoyBus init -------- */
//   gamepak_init();
//   build_cart_info();

// #ifndef ENABLE_CLI
//   /* -------- Build MSC virtual FAT image -------- */
//   fs_create_virtual_disk(gamepak_get_info(),
//                          g_info_file_buffer,
//                          strlen(g_info_file_buffer));
// #endif

//   /* -------- TinyUSB device stack --------------- */
//   tusb_init();

// #ifdef ENABLE_CLI
//   /* Wait for host to open CDC so CLI prints appear */
//   while (!tud_cdc_connected()) tud_task();

//   printf("\n--- pico-pak CLI mode ---\n");
//   plugin_n64_register();
//   cli_core_init();
// #else
//   printf("\n--- pico-pak MSC mode ---\n");
// #endif

//   while (true)
//   {
//     tud_task();                 /* USB housekeeping */

// #ifdef ENABLE_CLI
//     cli_core_task();
// #endif

//   }
// }




// /**
//  * main.c - pico-pak entry point
//  * Works in two build variants:
//  * - ENABLE_CLI defined:  Full interactive command-line interface.
//  * - ENABLE_CLI undefined: Simple USB Mass Storage device.
//  */
// #include "pico/stdlib.h"
// #include "tusb.h"
// #include <stdio.h>
// #include <string.h>

// #include "n64/devices/gamepak.h"

// #ifdef ENABLE_CLI
//   #include "cli/core.h"
//   void plugin_n64_register(void);
// #else
//   #include "usb/filesystem.h" // MSC only
// #endif

// // --- Global Buffers ---
// // The content for the virtual CART_INFO.TXT file.
// char g_info_file_buffer[512];

// // --- Helper Functions ---

// /**
//  * @brief Populates the global info buffer with cartridge details.
//  *
//  * This function checks the result of gamepak_init() and formats a string
//  * with either the cartridge details or an error message.
//  */
// static void build_cart_info_string(void) {
//     const n64_gamepak_info_t *info = gamepak_get_info();

//     // Case 1: gamepak_init() failed or no cartridge is present.
//     if (!info) {
//         snprintf(g_info_file_buffer, sizeof(g_info_file_buffer),
//             "--- GamePak Error! ---\r\n\r\n"
//             "Could not initialize the cartridge.\r\n"
//             "Please check if a cartridge is inserted correctly.\r\n"
//         );
//         return;
//     }

//     // Case 2: Cartridge was found and initialized successfully.
//     char title[21] = {0};
//     gamepak_get_rom_title(title, sizeof(title));

//     char game_id[5] = {0};
//     memcpy(game_id, info->header.game_id, 4);

//     char save_type_str[40];
//     switch (info->save_type) {
//         case N64_SAVE_TYPE_SRAM:       sprintf(save_type_str, "SRAM (%u KB)", info->save_size_bytes / 1024); break;
//         case N64_SAVE_TYPE_EEPROM_4K:  sprintf(save_type_str, "EEPROM 4Kbit"); break;
//         case N64_SAVE_TYPE_EEPROM_16K: sprintf(save_type_str, "EEPROM 16Kbit"); break;
//         case N64_SAVE_TYPE_FLASHRAM:   sprintf(save_type_str, "FlashRAM (%u KB)", info->save_size_bytes / 1024); break;
//         default:                       strcpy(save_type_str, "None"); break;
//     }

//     snprintf(g_info_file_buffer, sizeof(g_info_file_buffer),
//         "--- N64 Cartridge Info ---\r\n\r\n"
//         "Title: %s\r\n"
//         "ID:    %s\r\n"
//         "Save:  %s\r\n"
//         "CRC1:  %08X\r\n"
//         "CRC2:  %08X\r\n",
//         title, game_id, save_type_str, info->header.crc1, info->header.crc2
//     );
// }

// // --- Main Application Entry ---

// int main(void) {
//     // --- 1. Core Hardware and I/O Initialization ---
//     // Initialize stdio for printf over USB CDC (or UART depending on config).
//     stdio_init_all();
//     // Initialize the N64 cartridge interface hardware.
//     gamepak_init();

//     // --- 2. Prepare Dynamic Data ---
//     // Create the content for the virtual CART_INFO.TXT file based on detection results.
//     build_cart_info_string();

//     // --- 3. Mode-Specific Setup ---
// #ifdef ENABLE_CLI
//     // For CLI mode, we just need to register the commands.
//     plugin_n64_register();
//     cli_core_init();
//     // printf("\n--- pico-pak CLI Mode Initialized ---\n");
// #else
//     // For MSC mode, build the virtual disk image in memory before starting USB.
//     fs_create_virtual_disk(gamepak_get_info(), g_info_file_buffer, strlen(g_info_file_buffer));
//     // printf("\n--- pico-pak MSC Mode Initialized ---\n");
// #endif

//     // --- 4. Start the USB Device Stack ---
//     // This makes the device appear to the host computer.
//     tusb_init();

//     // --- 5. Main Application Loop ---
//     while (true) {
//         // TinyUSB's main task function must be called continuously.
//         tud_task();

//     #ifdef ENABLE_CLI
//         // In CLI mode, run the command line processing task.
//         cli_core_task();
//     #endif
//         // In MSC mode, the loop is idle. All operations are handled
//         // by TinyUSB's callbacks, which are invoked by tud_task().
//     }
// }




/*
 *  main.c  – pico-pak entry point
 *  Works in two build variants:
 *     • ENABLE_CLI defined  → composite USB (CDC+MSC) + CLI console
 *     • ENABLE_CLI undefined→ mass-storage only
 */

#include "pico/stdlib.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>

#include "n64/devices/gamepak.h"

#ifdef ENABLE_CLI
  #include "cli/core.h"
  void plugin_n64_register(void);
#else
  #include "usb/filesystem.h"           /* MSC only */
#endif

#ifndef ENABLE_CLI          /* MSC build only needs them */
extern uint8_t *flash_cache;
extern uint32_t flash_max_written;
extern bool     flash_write_pending;
#endif


/* ---------------- CART_INFO.TXT (always needed) ------------------------- */
char g_info_file_buffer[512];

/* ---------------- Helpers ------------------------------------------------ */
static void build_cart_info(void)
{
  const n64_gamepak_info_t *inf = gamepak_get_info();
  if (!inf) {
    strcpy(g_info_file_buffer,
           "--- GamePak Error! ---\r\n\r\nCartridge not detected.\r\n");
    return;
  }

  char title[21] = {0}; gamepak_get_rom_title(title, sizeof title);
  char gid[5]    = {0}; memcpy(gid, inf->header.game_id, 4);

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
           "--- N64 Cartridge Info ---\r\n\r\n"
           "Title: %s\r\nID:    %s\r\nSave:  %s\r\n"
           "CRC1:  %08X\r\nCRC2:  %08X\r\n",
           title, gid, save, inf->header.crc1, inf->header.crc2);
}

/* ----------------------------------------------------------------------- */
int main(void)
{
  stdio_init_all();

  /* -------- N64 cartridge / JoyBus init -------- */
  gamepak_init();
  build_cart_info();

#ifndef ENABLE_CLI
  /* -------- Build MSC virtual FAT image -------- */
  fs_create_virtual_disk(gamepak_get_info(),
                         g_info_file_buffer,
                         strlen(g_info_file_buffer));
#endif

  /* -------- TinyUSB device stack --------------- */
  tusb_init();

#ifdef ENABLE_CLI
  /* Wait for host to open CDC so CLI prints appear */
  while (!tud_cdc_connected()) tud_task();

  printf("\n--- pico-pak CLI mode ---\n");
  plugin_n64_register();
  cli_core_init();
#else
  printf("\n--- pico-pak MSC mode ---\n");
#endif

  /* ---- Flash-RAM staging flags are defined inside msc_disk.c ---- */
  extern bool     flash_write_pending;
  extern uint8_t *flash_cache;

  while (true)
  {
    tud_task();                 /* USB housekeeping */

#ifdef ENABLE_CLI
    cli_core_task();
#endif

#ifndef ENABLE_CLI
    /* ---------- Finish Flash-RAM write after host is done ---------- */
    if (flash_write_pending)
    {
      printf("Programming FlashRAM block …\n");

      bool ok = gamepak_write_flashram_sector(0, flash_cache);
      printf(ok ? "Flash save verified OK\n"
                : "Flash save VERIFY FAILED\n");

      flash_write_pending = false;
      /* free(flash_cache); flash_cache = NULL; */ /* keep or free */
    }
#endif
  }
}