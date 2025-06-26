/**
 * @file    gamepak.h
 * @brief   N64 GamePak (cartridge) interface definitions and accessors.
 */

#ifndef N64_GAMEPAK_H
#define N64_GAMEPAK_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h> // For size_t
#include <string.h> // For memset and memcpy
#include <stdbool.h> // For bool type

//------------------------------------------------------------------------------
// Memory Map and Sizes
//------------------------------------------------------------------------------

/// Base address for GamePak ROM reads
#define N64_GAMEPAK_ROM_BASE        0x10000000u
/// Base address for GamePak SRAM reads/writes
#define N64_GAMEPAK_SRAM_BASE       0x08000000u

/** Cart read chunk size (bytes) */
#define N64_CART_CHUNK_BYTES            (512u)

/// Size of the on-cart GamePak header
#define N64_GAMEPAK_ROM_SIZE        (32u * 1024 * 1024)
#define N64_GAMEPAK_HEADER_SIZE     64u
/// Offset of the ASCII title within the header
#define N64_GAMEPAK_TITLE_OFFSET    0x20u
/// Length of the ASCII title (20 bytes, no NUL on-disk)
#define N64_GAMEPAK_TITLE_LENGTH    20u
/// Offsets of the two CRC fields in the header
#define N64_GAMEPAK_CRC1_OFFSET     0x10u
#define N64_GAMEPAK_CRC2_OFFSET     0x14u
#define N64_GAMEPAK_CRC_CHUNK_SIZE    512

/// Number of bytes in one page/block of SRAM
#define N64_GAMEPAK_SRAM_PAGE_SIZE  512u
#define N64_GAMEPAK_SRAM_SIZE       (32 * 1024u)
#define N64_GAMEPAK_EEPROM_SIZE     512u

#define SRAM_SIZE_BYTES    (32 * 1024u)   // 256 Kbit
#define FLASHRAM_SIZE_BYTES (128 * 1024u) // 1 Mbit = 128 KiB
#define EEPROM_4K_SIZE_BYTES  (4 * 1024u / 8u)   // 4 Kbit = 512 B
#define EEPROM_16K_SIZE_BYTES (16 * 1024u / 8u)  // 16 Kbit = 2 KiB

//------------------------------------------------------------------------------
// Save Media Types
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
// Header Structures
//------------------------------------------------------------------------------

/**
 * @struct n64_gamepak_header_t
 * @brief  Exact 64-byte on-cart header layout (packed, no padding).
 */
typedef struct __attribute__((packed)) {
    uint32_t initial_settings;                // 0x00: Initial PI settings (e.g., 0x80371240 for Big Endian)
    uint32_t clock_rate;                      // 0x04: Clock Rate or PI_BSD_DOM1_REGC (often 0xFFFFFFFF for default)
    uint32_t pc_start;                        // 0x08: Program Counter Start Address
    uint32_t release_addr;                    // 0x0C: Release Address (often unused in commercial carts)
    uint32_t crc1;                            // 0x10: CRC1 (Checksum 1)
    uint32_t crc2;                            // 0x14: CRC2 (Checksum 2)
    uint8_t  _reserved1[8];                   // 0x18: Reserved/Unused bytes (8 bytes)
    char     title[N64_GAMEPAK_TITLE_LENGTH]; // 0x20: Internal Name (20 bytes, Shift-JIS or ASCII, padded)
    uint8_t  _reserved2[4];                   // 0x34: Reserved/Unused bytes (4 bytes)
    uint16_t game_id;                         // 0x38: Game ID (2 bytes, alphanumeric, e.g., 'ZL' for Zelda)
    uint8_t  format_type;                     // 0x3A: Format Type ('N' for cart, 'D' for 64DD, 'C' for combo, etc.)
    uint8_t  version;                         // 0x3B: Version (e.g., 0x00 for 1.0, 0x10 for 1.1)
    uint8_t  country_code;                    // 0x3C: Country Code (e.g., 0x45 for USA, 0x4A for Japan)
    uint8_t  _reserved3[3];                   // 0x3D: Reserved/Unused bytes (3 bytes to fill to 0x40)
} n64_gamepak_header_t;

/**
 * @struct n64_gamepak_info_t
 * @brief  Aggregated GamePak info (header + status + save type).
 */
typedef struct {
    n64_gamepak_header_t header; /**< Raw header bytes, mapped to fields */
    bool                 valid;  /**< True if header was successfully read */
    n64_save_type_t      save_type; /**< Detected save media type */
} n64_gamepak_info_t;

//------------------------------------------------------------------------------
// Initialization & Header Access
//------------------------------------------------------------------------------

/**
 * @brief Initialize AD-bus and read the GamePak header.
 * @return true on success, false on failure.
 */
bool gamepak_init(void);

/**
 * @brief  Get pointer to the global GamePak info.
 * @note   Returns NULL if init() has not been called or failed.
 */
const n64_gamepak_info_t *gamepak_get_info(void);
const n64_gamepak_header_t *gamepak_get_header(void);
bool gamepak_is_valid(void);

//------------------------------------------------------------------------------
// Header Field Getters
//------------------------------------------------------------------------------

/** @brief  Read raw 64-byte header into a caller buffer. */
void        gamepak_read_header(uint8_t buffer[N64_GAMEPAK_HEADER_SIZE]);

/** @brief  Get CRC1 value from the header. */
uint32_t    gamepak_get_crc1(void);

/** @brief  Get CRC2 value from the header. */
uint32_t    gamepak_get_crc2(void);

/** @brief  Get a NUL-terminated pointer to the title string. */
const char *gamepak_get_title(void);

/** @brief  Get the country/region code. */
uint8_t     gamepak_get_country_code(void);

/** @brief  Get the header version byte. */
uint8_t     gamepak_get_version(void);

//------------------------------------------------------------------------------
// ROM Read Utilities
//------------------------------------------------------------------------------

/**
 * @brief Read arbitrary bytes from GamePak ROM.
 * @param base_addr  Starting ROM address.
 * @param buf        Destination buffer.
 * @param len        Number of bytes to read (must be even).
 * @return true on success.
 */
bool gamepak_read_bytes(uint32_t base_addr, uint8_t *buf, size_t len);

/**
 * @brief Fast burst read from GamePak ROM (no per-word delays).
 * @param base_addr  Starting ROM address.
 * @param buf        Destination buffer.
 * @param len        Number of bytes to read (must be even).
 * @return true on success.
 */
bool gamepak_read_bytes_fast(uint32_t base_addr, uint8_t *buf, size_t len);

//------------------------------------------------------------------------------
// SRAM (SavePak) Functions
//------------------------------------------------------------------------------

/** @brief Check if the inserted GamePak has battery-backed SRAM. */
bool     gamepak_has_sram(void);

/** @brief Read one 16-bit word from GamePak SRAM. */
uint16_t gamepak_read_sram_word(uint32_t addr);
/** @brief Write one 16-bit word to GamePak SRAM. */
bool     gamepak_write_sram_word(uint32_t addr, uint16_t data);

/** @brief Read a 512-byte SRAM page into the given buffer. */
bool     gamepak_read_sram_page(uint32_t page_addr, uint8_t *buf);

/** @brief Dump the entire SRAM contents to stdout. */
bool     gamepak_dump_sram(void);

#endif // N64_GAMEPAK_H
