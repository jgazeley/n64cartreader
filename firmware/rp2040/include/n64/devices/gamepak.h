/**
 * @file     gamepak.h
 * @brief    A comprehensive, clean API for the N64 GamePak (cartridge).
 * @version  2.0
 */
#ifndef N64_GAMEPAK_H
#define N64_GAMEPAK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// #define N64_ROM_BASE              0x10000000u
// #define N64_SRAM_BASE             0x08000000u
// #define N64_HEADER_SIZE           64u
// #define N64_SAVE_PAGE_BUFFER_SIZE 512u
// #define N64_SRAM_SIZE             (32 * 1024u)
// #define N64_EEPROM_4K_SIZE        512u
// #define N64_EEPROM_16K_SIZE       2048u
// #define N64_FLASHRAM_SIZE         (128 * 1024u)
// #define FLASHRAM_READ_CHUNK_SIZE  128
// #define FLASHRAM_CMD_REG          (N64_SRAM_BASE + 0x010000)
// #define FLASHRAM_READ_CMD         0xF0000000u
// #define FLASHRAM_READ_ARRAY_CMD   0xF0000000u
// #define FLASHRAM_RESET_CMD        0xFF000000u
// #define FLASHRAM_STATUS_CMD       0x70000000u
// #define FLASHRAM_ID_CMD           0x90000000u
// #define FLASHRAM_SET_STATUS_MODE_CMD 0xE1000000u

// #define FLASHRAM_STATUS_READY     0x8080
// #define FLASHRAM_MFG_MACRONIX     0xC2
// #define FLASHRAM_MFG_SHARP        0xB0

//==============================================================================
// Constants and Core Types
//==============================================================================

// ---------------------------- Cartridge address map --------------------------
#define N64_ROM_BASE                0x10000000u   // Parallel bus (ROM)
#define N64_SRAM_BASE               0x08000000u   // Parallel bus (save area)

// ---------------------------- Fixed sizes ------------------------------------
#define N64_HEADER_SIZE             64u
#define N64_SAVE_PAGE_BUFFER_SIZE   512u          // Our in-RAM mirror

// Save-device capacities
#define N64_SRAM_SIZE               (32 * 1024u)     // 32 KB
#define N64_EEPROM_4K_SIZE          512u              // 4 Kbit
#define N64_EEPROM_16K_SIZE         2048u             // 16 Kbit
#define N64_FLASHRAM_SIZE           (128 * 1024u)     // 1 Mbit (128 KB)

// ---------------------------- FlashRAM layout --------------------------------
#define FLASHRAM_BLOCK_SIZE         (128 * 1024u)     // one erase block
#define FLASHRAM_SECTOR_SZ             512
#define FLASHRAM_PAGE_SIZE          128u              // page-program size
#define FLASHRAM_CMD_REG            (N64_SRAM_BASE + 0x010000)
#define FLASHRAM_SECTOR_CNT         (FLASHRAM_BLOCK_SIZE / FLASHRAM_SECTOR_SZ)   /* 256 */

// ---------------------------- FlashRAM commands ------------------------------
#define FLASHRAM_READ_ARRAY_CMD     0xF0000000u   // same as READ_CMD
/* 0xF0000000 appears twice in the old list under READ_CMD and READ_ARRAY_CMD.
   Kept one canonical name; retire the other in code.                     */

#define FLASHRAM_RESET_CMD          0xFF000000u
#define FLASHRAM_STATUS_CMD         0x70000000u   // unused so far
#define FLASHRAM_ID_CMD             0x90000000u   // unused (we read ID via status)
#define FLASHRAM_SET_STATUS_MODE_CMD 0xE1000000u

/* New for write support */
#define FLASHRAM_ERASE_CMD          0x4B000000u   // + word addr bits
#define FLASHRAM_PROGRAM_CMD        0xB4000000u   // + word addr bits
#define FLASHRAM_ERASE_MODE_CMD     0x78000000u   /* extra step before EXECUTE */
#define FLASHRAM_PROGRAM_OFFSET_CMD 0xA5000000u   /* page-index register       */

#define FLASHRAM_EXECUTE_CMD        0xD2000000u   // commit queued op

// ---------------------------- Status signatures ------------------------------
#define FLASHRAM_STATUS_READY       0x8080        // legacy 16-bit code
#define FLASHRAM_IDLE_STATUS8       {0x11,0x11,0x80,0x01,0x00,0xC2,0x00,0x1E}
/* Accept any of the three legit 8-byte idle signatures */
static const uint8_t FLASH_IDLE_MX1100[8] = {0x11,0x11,0x80,0x01,0x00,0xC2,0x00,0x1E};
static const uint8_t FLASH_IDLE_MX1101[8] = {0x11,0x11,0x80,0x01,0x00,0xC2,0x00,0x1D};
static const uint8_t FLASH_IDLE_MN63F81[8] = {0x11,0x11,0x80,0x01,0x00,0x32,0x00,0xF1};


// Manufacturer IDs
#define FLASHRAM_MFG_MACRONIX       0xC2
#define FLASHRAM_MFG_SHARP          0xB0
/* Panasonic (MN63F81) returns 0xB0 for MFG and 0xF1 for DEV; handled in code */

// ---------------------------- Misc helpers -----------------------------------
#define FLASHRAM_WORD_ADDR(byte_addr)   ((byte_addr) >> 1)   // cart quirk

/**
 * @struct n64_gamepak_header_t
 * @brief  Exact 64-byte on-cart header layout (packed, no padding).
 */
typedef struct __attribute__((packed)) {
    uint32_t initial_settings;    // 0x00: PI_BSB/PI_BSD initial settings (e.g. 0x80371240)
    uint32_t clock_rate;          // 0x04: Clock Rate override (0 = default)
    uint32_t pc_start;            // 0x08: Entry point (RAM address)
    uint32_t release_addr;        // 0x0C: Release (warm‐reset) address
    uint32_t crc1;                // 0x10: CRC1 (4-byte checksum)
    uint32_t crc2;                // 0x14: CRC2 (4-byte checksum)
    uint8_t  reserved1[8];        // 0x18–0x1F: unused
    char     title[20];           // 0x20–0x33: internal name (ASCII/Shift-JIS, padded)
    uint8_t  reserved2[7];        // 0x34–0x3A: unused padding
    char     game_id[4];          // 0x3B–0x3E: “CZGE”, “NGEE”, etc.
    uint8_t  version;             // 0x3F: cart revision (often 0x00)
} n64_gamepak_header_t;

/**
 * @enum n64_save_type_t
 * @brief   Types of non-volatile save media on N64 GamePaks.
 */
typedef enum {
    N64_SAVE_TYPE_NONE       = 0,  /**< No save media present */
    N64_SAVE_TYPE_SRAM       = 1,  /**< 256 Kbit battery-backed SRAM */
    N64_SAVE_TYPE_FLASHRAM   = 2,  /**< 1 Mbit FlashRAM */
    N64_SAVE_TYPE_EEPROM_4K  = 3,  /**< 4 Kbit EEPROM */
    N64_SAVE_TYPE_EEPROM_16K = 4,  /**< 16 Kbit EEPROM */
    N64_SAVE_TYPE_UNKNOWN    = 5   /**< Detected, but unrecognized */
} n64_save_type_t;

/**
 * @struct n64_gamepak_info_t
 * @brief  Aggregated GamePak info (header + status + save type).
 */
typedef struct {
    n64_gamepak_header_t header;      /**       < Raw header bytes, mapped to fields */
    bool                 valid;       /**       < True if header was successfully read */
    n64_save_type_t      save_type;   /**       < Detected save media type */
    size_t               save_size_bytes; /**   < FULL size of the save media in bytes */
    uint32_t             rom_size_bytes; /**    < FULL size of the ROM in bytes */
} n64_gamepak_info_t;

/** @brief Gets a read-only pointer to the pre-loaded 512-byte save data page. */
const uint8_t* gamepak_get_save_page_buffer(void);


//==============================================================================
// Initialization and Status
//==============================================================================

/** @brief Initializes all GamePak subsystems, reads header, and detects save type. */
bool gamepak_init(void);

/** @brief Safely checks if a cartridge is still present. Useful for hot-swap detection. */
bool gamepak_is_present(void);

/** @brief Gets a pointer to the main info struct containing all cartridge data. */
const n64_gamepak_info_t* gamepak_get_info(void);

/** @brief Checks if a valid GamePak (cartridge) is inserted. */
bool gamepak_is_valid(void);


//==============================================================================
// Cartridge Information (Header) Accessors
//==============================================================================

/** @brief Gets a direct pointer to the 64-byte header data. */
const n64_gamepak_header_t* gamepak_get_header(void);

/** @brief Gets a read-only pointer to the pre-loaded 512-byte save data page. */
const uint8_t* gamepak_get_save_page_buffer(void); // <-- ADD THIS LINE

/** @brief Gets the game title from the header, copies to a buffer, and trims spaces. */
void gamepak_get_rom_title(char* buffer, size_t buffer_len);

/** @brief Gets the game's save type enum. */
n64_save_type_t gamepak_get_save_type(void);

/** @brief Gets the game's save size in bytes. */
size_t gamepak_get_save_size(void);

/** @brief Gets CRC1 value from the header. */
uint32_t  gamepak_get_crc1(void);

/** @brief Gets CRC2 value from the header. */
uint32_t  gamepak_get_crc2(void);

/** @brief Gets the 4-character Game ID (e.g. "NGEE", "CZGE").  
 *  
 *  Returns a pointer to a 4-byte field (not NUL-terminated).  
 *  If you need to print it as a C-string, copy it into a 5-byte buffer and NUL-terminate.  
 */
char *gamepak_get_game_id(void);

/** @brief Gets the header version byte (offset 0x3F). */
uint8_t   gamepak_get_version(void);

//==============================================================================
// ROM Access Functions
//==============================================================================

/** @brief Reads a single 16-bit word from the ROM. */
uint16_t gamepak_read_rom_word(uint32_t rom_address);

/** @brief Reads an arbitrary number of bytes from the ROM into a buffer. */
bool gamepak_read_rom_bytes(uint32_t rom_address, uint8_t* buffer, size_t length);

// Note: Writing to ROM is not possible on standard carts, but these functions
// could be implemented for development/flash cartridges.
// bool gamepak_write_rom_word(uint32_t rom_address, uint16_t value);
// bool gamepak_write_rom_bytes(uint32_t rom_address, const uint8_t* buffer, size_t length);


//==============================================================================
// SRAM Access Functions
//==============================================================================

/** @brief Checks if the cartridge has SRAM. */
bool gamepak_has_sram(void);

/** @brief Reads a single 16-bit word from SRAM. */
uint16_t gamepak_read_sram_word(uint32_t sram_address);

/** @brief Writes a single 16-bit word to SRAM. */
bool gamepak_write_sram_word(uint32_t sram_address, uint16_t value);

/** @brief Reads an arbitrary number of bytes from SRAM into a buffer. */
bool gamepak_read_sram_bytes(uint32_t sram_address, uint8_t* buffer, size_t length);

/** @brief Writes a buffer of bytes to SRAM. */
bool gamepak_write_sram_bytes(uint32_t sram_address, const uint8_t* buffer, size_t length);


//==============================================================================
// EEPROM Access Functions
//==============================================================================

/** @brief Checks if the cartridge has a responsive EEPROM. */
bool gamepak_has_eeprom(void);

/** @brief Reads an arbitrary number of bytes from EEPROM into a buffer. */
bool gamepak_read_eeprom_bytes(uint32_t address, uint8_t* buffer, size_t length);

/** @brief Writes a buffer of bytes to EEPROM with read-after-write verification. */
bool gamepak_write_and_verify_eeprom_bytes(uint32_t address, const uint8_t* buffer, size_t length);


//==============================================================================
// FlashRAM Access Functions (Stubs for Future Expansion)
//==============================================================================

/** @brief Checks if the cartridge has FlashRAM. */
bool gamepak_has_flashram(void);

/** @brief Reads bytes from FlashRAM. */
bool gamepak_read_flashram_bytes(uint32_t address, uint8_t* buffer, size_t length);

/** @brief Writes bytes to a sector of FlashRAM. */
bool gamepak_write_flashram_sector(uint32_t address, const uint8_t* buffer);

bool gamepak_write_flashram_bytes(uint32_t addr, const uint8_t *src, size_t len);


#endif // N64_GAMEPAK_H

//------------------------------------------------------------------------------
// Initialization & Header Access
//------------------------------------------------------------------------------

// /**
//  * @brief Initialize AD-bus and read the GamePak header.
//  * @return true on success, false on failure.
//  */
// bool gamepak_init(void);

/**
 * @brief  Get pointer to the global GamePak info.
 * @note   Returns NULL if init() has not been called or failed.
 */
// const n64_gamepak_info_t    *gamepak_get_info(void);
// const n64_gamepak_header_t  *gamepak_get_header(void);
// bool gamepak_is_valid(void);

//------------------------------------------------------------------------------
// Header Field Getters
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// ROM Read Utilities
//------------------------------------------------------------------------------

// /**
//  * @brief Read arbitrary bytes from GamePak ROM.
//  * @param base_addr  Starting ROM address.
//  * @param buf        Destination buffer.
//  * @param len        Number of bytes to read (must be even).
//  * @return true on success.
//  */
// bool gamepak_read_bytes(uint32_t base_addr, uint8_t *buf, size_t len);

// /**
//  * @brief Fast burst read from GamePak ROM (no per-word delays).
//  * @param base_addr  Starting ROM address.
//  * @param buf        Destination buffer.
//  * @param len        Number of bytes to read (must be even).
//  * @return true on success.
//  */
// bool gamepak_read_bytes_fast(uint32_t base_addr, uint8_t *buf, size_t len);

// //------------------------------------------------------------------------------
// // SRAM (SavePak) Functions
// //------------------------------------------------------------------------------

// /** @brief Check if the inserted GamePak has battery-backed SRAM. */
// bool     gamepak_has_sram(void);

// /** @brief Read one 16-bit word from GamePak SRAM. */
// uint16_t gamepak_read_sram_word(uint32_t addr);
// /** @brief Write one 16-bit word to GamePak SRAM. */
// bool     gamepak_write_sram_word(uint32_t addr, uint16_t data);

// /** @brief Read a 512-byte SRAM page into the given buffer. */
// bool     gamepak_read_sram_page(uint32_t page_addr, uint8_t *buf);

// /** @brief Dump the entire SRAM contents to stdout. */
// bool     gamepak_dump_sram(void);

// #endif // N64_GAMEPAK_H
