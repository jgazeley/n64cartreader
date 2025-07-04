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





// #include "pico/stdlib.h"
// #include "tusb.h"
// #include <stdio.h> // Keep for possible future dedicated debug UART or just stub
// #include "n64/devices/gamepak.h" // Assumes this provides gamepak_read_rom_bytes and N64_ROM_BASE
// #include <string.h> // For strlen, strncmp

// #ifdef ENABLE_CLI
//   #include "cli/core.h"
//   #include "cli/menu.h" // For menu_render()
//   void plugin_n64_register(void);
// #else
//   #include "utils/packet.h" // Assumes PACKET_WRITE_CHUNK_BYTES is defined here
//   #include "utils/transport.h" // Assumes transport_init is here
// #endif


// // ──────────────────────────────────────────────────────────────────
// // Host-Pico Handshake Protocol Definitions (MUST MATCH HOST)
// // ──────────────────────────────────────────────────────────────────
// #define N64_ID_REQ_STR         "N64_ID_REQ\r\n"
// #define N64_ID_ACK_STR         "N64_DUMPER_V1.0_OK\r\n"
// #define N64_GET_HEADER_REQ_STR "N64_GET_HEADER\r\n"
// #define N64_DUMP_ROM_CMD_CHAR  'G' // Or 'g'

// #define N64_ROM_HEADER_SIZE    0x40 // 64 bytes

// // ──────────────────────────────────────────────────────────────────
// // Pico Protocol State Machine
// // ──────────────────────────────────────────────────────────────────
// typedef enum {
//     PICO_STATE_IDLE,                 // Waiting for initial ID request
//     PICO_STATE_AWAIT_HEADER_REQ,     // Sent ACK, waiting for header request
//     PICO_STATE_AWAIT_DUMP_CMD        // Sent header, waiting for dump 'G' command
// } pico_protocol_state_t;

// static pico_protocol_state_t s_pico_state = PICO_STATE_IDLE;

// // ──────────────────────────────────────────────────────────────────
// // Global Buffer for Incoming Commands
// // ──────────────────────────────────────────────────────────────────
// #define MAX_INCOMING_CMD_LEN 64 // Max length of expected command strings including terminators
// static char s_cmd_buf[MAX_INCOMING_CMD_LEN];
// static size_t s_cmd_buf_idx = 0;

// // ──────────────────────────────────────────────────────────────────
// // Helper Functions for CDC Communication
// // ──────────────────────────────────────────────────────────────────

// // Helper to send a null-terminated string over CDC
// static void cdc_send_string(const char *str) {
//     size_t len = strlen(str);
//     if (len > 0) {
//         tud_cdc_write(str, len);
//         tud_cdc_write_flush();
//         // printf("PICO TX: %s", str); // Debug via separate UART if needed, not USB CDC
//     }
// }

// // Helper to send raw bytes over CDC
// static void cdc_send_bytes(const uint8_t *buf, size_t len) {
//     if (len > 0) {
//         tud_cdc_write(buf, len);
//         tud_cdc_write_flush();
//         // printf("PICO TX: Sent %zu bytes.\n", len); // Debug via separate UART if needed, not USB CDC
//     }
// }

// // Helper to read available bytes into command buffer
// // Returns true if buffer contains a known command, false otherwise.
// static bool cdc_read_and_process_command(void) {
//     while (tud_cdc_available() && s_cmd_buf_idx < (MAX_INCOMING_CMD_LEN - 1)) {
//         s_cmd_buf[s_cmd_buf_idx++] = tud_cdc_read_char();
//         s_cmd_buf[s_cmd_buf_idx] = '\0'; // Null-terminate for string comparison

//         // Check for complete command strings based on current state
//         switch (s_pico_state) {
//             case PICO_STATE_IDLE:
//                 if (s_cmd_buf_idx >= strlen(N64_ID_REQ_STR) &&
//                     strncmp(s_cmd_buf, N64_ID_REQ_STR, strlen(N64_ID_REQ_STR)) == 0) {
//                     return true;
//                 }
//                 break;
//             case PICO_STATE_AWAIT_HEADER_REQ:
//                 if (s_cmd_buf_idx >= strlen(N64_GET_HEADER_REQ_STR) &&
//                     strncmp(s_cmd_buf, N64_GET_HEADER_REQ_STR, strlen(N64_GET_HEADER_REQ_STR)) == 0) {
//                     return true;
//                 }
//                 break;
//             case PICO_STATE_AWAIT_DUMP_CMD:
//                 // For a single character command like 'G', just check the first byte
//                 if (s_cmd_buf_idx > 0 && (s_cmd_buf[0] == N64_DUMP_ROM_CMD_CHAR || s_cmd_buf[0] == (N64_DUMP_ROM_CMD_CHAR + ('a'-'A')))) {
//                     return true;
//                 }
//                 break;
//         }
//     }
//     // If buffer is full and no command matched, or no more data, reset for next attempt
//     if (s_cmd_buf_idx >= (MAX_INCOMING_CMD_LEN - 1) && !tud_cdc_available()) {
//          // printf("PICO: RX buffer full, no match. Resetting. (State: %d)\n", s_pico_state); // Debug
//          s_cmd_buf_idx = 0; // Reset buffer
//          memset(s_cmd_buf, 0, MAX_INCOMING_CMD_LEN);
//     }
//     return false; // No complete command yet
// }

// // ──────────────────────────────────────────────────────────────────
// // GamePak Streaming Function (unchanged, but now called by new logic)
// // ──────────────────────────────────────────────────────────────────
// static bool stream_gamepak(uint32_t base, size_t total) {
//     // Note: PACKET_WRITE_CHUNK_BYTES must be defined, e.g., in packet.h
//     // Assuming PACKET_WRITE_CHUNK_BYTES is defined or using a default for tud_cdc_write
//     #ifndef PACKET_WRITE_CHUNK_BYTES
//     #define PACKET_WRITE_CHUNK_BYTES 512 // Default if not defined by packet.h
//     #endif

//     static uint8_t buf[PACKET_WRITE_CHUNK_BYTES];
//     size_t sent = 0;

//     // printf("PICO: Starting ROM dump from 0x%08lX, total %zu bytes...\n", base, total); // Debug

//     while (sent < total) {
//         size_t chunk = PACKET_WRITE_CHUNK_BYTES;
//         if (sent + chunk > total) {
//             chunk = total - sent;
//         }

//         // Use the clean GamePak API to read the ROM data
//         if (!gamepak_read_rom_bytes(base + sent, buf, chunk)) {
//             // printf("PICO ERROR: Failed to read from GamePak at address 0x%08lX\n", base + sent); // Debug
//             return false;
//         }

//         // Use tud_cdc_write directly to send the data
//         cdc_send_bytes(buf, chunk); // Using our helper which flushes
//         sent += chunk;
//     }
//     // printf("PICO: ROM Dump complete. Sent %zu bytes.\n", sent); // Debug
//     return true;
// }


// // ──────────────────────────────────────────────────────────────────
// // Main Entry Point
// // ──────────────────────────────────────────────────────────────────
// int main(void) {
//     // 1. Common Initialization for both modes
//     tusb_init();
//     stdio_usb_init(); // Keep this, as it initializes the USB CDC

//     // 2. Wait for USB serial connection to be established
//     while (!tud_cdc_connected()) {
//         tud_task();
//         sleep_ms(10);
//     }

//     #ifndef ENABLE_CLI
//         // transport_init(); // From utils/transport.h if enabled - if it prints, might need disabling too
//     #endif

//     // 3. Initialize the GamePak hardware
//     // printf("\n--- System Boot. Initializing GamePak... ---\n\n"); // Debug
//     if (!gamepak_init()) {
//         // printf("PICO ERROR: GamePak failed to initialize. Check cartridge is inserted.\n"); // Debug
//         // Don't halt, allow ID check even if no cart, but dumps will fail.
//     } else {
//         // printf("PICO SUCCESS: GamePak initialized.\n"); // Debug
//     }
//     // printf("PICO Mode: Streamer-Only. Awaiting Host Commands.\n\n"); // Debug


//     // The main application loop
//     while (1) {
//         tud_task(); // Required for USB
//         // tud_cdc_rx_flush(); // Optional: uncomment if stale data causes issues

//     #ifdef ENABLE_CLI
//         // --- CLI Mode Logic ---
//         cli_core_task();
//     #else
//         // --- Streamer-Only Mode Logic (NEW PROTOCOL) ---
//         if (cdc_read_and_process_command()) {
//             // A complete command has been received and identified
//             if (s_pico_state == PICO_STATE_IDLE && strncmp(s_cmd_buf, N64_ID_REQ_STR, strlen(N64_ID_REQ_STR)) == 0) {
//                 // printf("PICO RX: ID Request received.\n"); // Debug
//                 cdc_send_string(N64_ID_ACK_STR);
//                 s_pico_state = PICO_STATE_AWAIT_HEADER_REQ;
//             } else if (s_pico_state == PICO_STATE_AWAIT_HEADER_REQ && strncmp(s_cmd_buf, N64_GET_HEADER_REQ_STR, strlen(N64_GET_HEADER_REQ_STR)) == 0) {
//                 // printf("PICO RX: Get Header Request received.\n"); // Debug
//                 uint8_t header_buf[N64_ROM_HEADER_SIZE];
//                 // Read 0x40 bytes from ROM base address 0x00000000 for the header
//                 if (gamepak_read_rom_bytes(N64_ROM_BASE, header_buf, N64_ROM_HEADER_SIZE)) {
//                     cdc_send_bytes(header_buf, N64_ROM_HEADER_SIZE);
//                     // printf("PICO: Sent ROM header (%u bytes).\n", N64_ROM_HEADER_SIZE); // Debug
//                     s_pico_state = PICO_STATE_AWAIT_DUMP_CMD; // Ready for dump command
//                 } else {
//                     // printf("PICO ERROR: Failed to read ROM header. Resetting state.\n"); // Debug
//                     s_pico_state = PICO_STATE_IDLE; // Error, go back to idle
//                     cdc_send_string("ERROR_HEADER_READ\r\n"); // Inform host of error
//                 }
//             } else if (s_pico_state == PICO_STATE_AWAIT_DUMP_CMD && (s_cmd_buf[0] == N64_DUMP_ROM_CMD_CHAR || s_cmd_buf[0] == (N64_DUMP_ROM_CMD_CHAR + ('a'-'A')))) {
//                 // printf("PICO RX: ROM Dump Command 'G' received.\n"); // Debug
//                 // Dump the first 1MB of the ROM (or N64_ROM_SIZE if defined)
//                 #ifdef N64_ROM_SIZE
//                 bool success = stream_gamepak(N64_ROM_BASE, N64_ROM_SIZE);
//                 #else
//                 bool success = stream_gamepak(N64_ROM_BASE, 1 * 1024 * 1024); // Default 1MB for example
//                 #endif
//                 if (success) {
//                     // printf("PICO: Full dump stream complete.\n"); // Debug
//                 } else {
//                     // printf("PICO: Dump stream failed.\n"); // Debug
//                     cdc_send_string("ERROR_DUMP_FAIL\r\r\n"); // Inform host of error (added \r to match host format)
//                 }
//                 s_pico_state = PICO_STATE_IDLE; // Dump complete, go back to idle for next session
//             } else {
//                 // printf("PICO: Unexpected command or state mismatch. Received: '%s', Current State: %d. Resetting.\n", s_cmd_buf, s_pico_state); // Debug
//                 cdc_send_string("ERROR_PROTOCOL_MISMATCH\r\n"); // Inform host
//                 s_pico_state = PICO_STATE_IDLE; // Error, go back to idle
//             }
//             // Reset buffer index after processing a command
//             s_cmd_buf_idx = 0;
//             memset(s_cmd_buf, 0, MAX_INCOMING_CMD_LEN);
//         }
//     #endif // !ENABLE_CLI
//     } // while(1)
// }






// /**
//  * @file main.c
//  * @brief Main entry point. Initializes CLI or USB Mass Storage mode.
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
//   // When CLI is disabled, we are in MSC mode. Include its headers.
//   #include "usb/filesystem.h"
// #endif

// // This is the actual global buffer for the CART_INFO.TXT file content.
// // It is declared here and used by msc_disk.c via an 'extern' declaration.
// char g_info_file_buffer[512];

// /**
//  * @brief Populates the global info buffer with cartridge details.
//  * * This function checks the result of gamepak_init() and formats a string
//  * with either the cartridge details or an error message.
//  */
// void generate_cart_info_string(void) {
//     const n64_gamepak_info_t *info = gamepak_get_info();

//     // Case 1: gamepak_init() was successful and found a cartridge.
//     if (info) {
//         char title_buf[21] = {0};
//         gamepak_get_rom_title(title_buf, sizeof(title_buf));

//         char id_buf[5] = {0};
//         memcpy(id_buf, info->header.game_id, 4);

//         char save_type_str[40];
//         switch (info->save_type) {
//             case N64_SAVE_TYPE_SRAM:     sprintf(save_type_str, "SRAM (%u KB)", info->save_size_bytes / 1024); break;
//             case N64_SAVE_TYPE_EEPROM_4K:  sprintf(save_type_str, "EEPROM 4Kbit"); break;
//             case N64_SAVE_TYPE_EEPROM_16K: sprintf(save_type_str, "EEPROM 16Kbit"); break;
//             case N64_SAVE_TYPE_FLASHRAM: sprintf(save_type_str, "FlashRAM (%u KB)", info->save_size_bytes / 1024); break;
//             default: strcpy(save_type_str, "None"); break;
//         }

//         snprintf(g_info_file_buffer, sizeof(g_info_file_buffer),
//             "--- N64 Cartridge Info ---\r\n\r\n"
//             "Title: %s\r\n"
//             "ID:    %s\r\n"
//             "Save:  %s\r\n"
//             "CRC1:  %08X\r\n"
//             "CRC2:  %08X\r\n",
//             title_buf, id_buf, save_type_str, info->header.crc1, info->header.crc2
//         );
//     } 
//     // Case 2: gamepak_init() failed.
//     else {
//         snprintf(g_info_file_buffer, sizeof(g_info_file_buffer),
//             "--- GamePak Error! ---\r\n\r\n"
//             "Could not initialize the cartridge.\r\n"
//             "Please check if a cartridge is inserted correctly.\r\n"
//         );
//     }
// }

// int main(void) {
//     // 1. Initialize stdio and the GamePak hardware.
//     stdio_init_all();
//     gamepak_init();

//     // 2. Generate the dynamic info string based on the result of gamepak_init().
//     generate_cart_info_string();

//     // 3. Initialize either the CLI or MSC mode depending on build flags.
// #ifdef ENABLE_CLI
//     // --- CLI MODE INITIALIZATION ---
//     tusb_init(); // Init TinyUSB for CDC
//     while (!tud_cdc_connected()) {
//         tud_task(); // Wait for a serial connection
//     }
//     printf("\n--- pico-pak CLI Mode Initialized ---\n");
//     plugin_n64_register();
//     cli_core_init();
// #else
//     // --- MASS STORAGE (MSC) MODE INITIALIZATION ---
//     printf("\n--- pico-pak MSC Mode Initialized ---\n");
//     // Build the virtual disk using the detected cart info
//     fs_create_virtual_disk(gamepak_get_info(), g_info_file_buffer, strlen(g_info_file_buffer));
//     tusb_init(); // Init TinyUSB for MSC + CDC
// #endif

//     // The main application loop
//     while (1) {
//         // tud_task() is always required for all USB activity
//         tud_task();

//     #ifdef ENABLE_CLI
//         // In CLI mode, run the command line task
//         cli_core_task();
//     #else
//         // In MSC mode, there's nothing to do in the main loop.
//         // All MSC operations are handled by TinyUSB's callbacks via tud_task().
//     #endif
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
