/**
 * @file     gamepak.c
 * @brief    Implementation of the GamePak (cartridge) API.
 * @version  2.0
 */

#include "n64/devices/gamepak.h" // Public API header
#include "n64/bus/adbus.h"      // Low-level hardware access
#include "n64/bus/joybus.h"     // For save game access

#include "pico/stdlib.h"        // For sleep_ms, etc.
//#include "tusb.h"             // < legacy, no longer needed?

#include <string.h>             // For memset, memcpy
#include <stdio.h>              // for printf() ///## DEBUG ##///
//#include <stdlib.h>           // < for malloc/free, no longer used?


//==============================================================================
// Private Module-Level State
//==============================================================================

static n64_gamepak_info_t s_gamepak_info;
static uint8_t s_save_page_buffer[N64_SAVE_PAGE_BUFFER_SIZE];
static uint32_t s_golden_header_value = 0; // For hot-swap detection

bool flashram_program_page(uint32_t byte_addr, const uint8_t data[FLASHRAM_PAGE_SIZE]);

//==============================================================================
// Private Helper Functions
//==============================================================================

// Private — called once during init, does the destructive probe
static bool _gamepak_probe_sram(void) {
    uint16_t test_val = 0x5A5A;
    uint32_t addr = N64_SRAM_BASE;

    uint16_t backup = gamepak_read_sram_word(addr);
    gamepak_write_sram_word(addr, test_val);
    uint16_t verify = gamepak_read_sram_word(addr);
    gamepak_write_sram_word(addr, backup);

    return (verify == test_val);
}

/**
 * @brief  Detects the N64 ROM size by checking for mirrored data.
 *
 * This function works by reading a fingerprint from the start of the ROM,
 * then checking every megabyte from the maximum size downwards. The first
 * time it finds data that is NOT a mirror of the start, it determines
 * that is the highest unique block of data, giving the correct size.
 * This correctly handles non-power-of-two ROM sizes like 12MB.
 *
 * @return The detected ROM size in bytes. Returns 0 on failure.
 */
static uint32_t _gamepak_detect_rom_size(void)
{
    const uint32_t MAX_BYTES = 64 * 1024 * 1024;
    const uint32_t PROBE_STEP = 1 * 1024 * 1024;   // probe every 1 MiB
    const uint32_t FINGERPRINT_LEN = 16;

    uint8_t base[FINGERPRINT_LEN];
    if (!gamepak_read_rom_bytes(N64_ROM_BASE, base, FINGERPRINT_LEN)) {
        return 0; // Read failure
    }

    for (uint32_t offs = PROBE_STEP; offs < MAX_BYTES; offs += PROBE_STEP) {
        uint8_t probe[FINGERPRINT_LEN];
        if (!gamepak_read_rom_bytes(N64_ROM_BASE + offs, probe, FINGERPRINT_LEN)) {
            // A bus error here likely means we've read past the physical chip end.
            // The previous offset was the last valid one, making `offs` the size.
            return offs;
        }

        if (memcmp(base, probe, FINGERPRINT_LEN) == 0) {
            // We've found a mirror. The real size is the current offset.
            return offs;
        }
    }
    
    // If we complete the loop without finding a mirror, it must be a full 64 MiB chip.
    return MAX_BYTES;
}

/**
 * @brief Probes for all known save types (SRAM, EEPROM, etc.) and pre-loads a page.
 * * This function is called once during gamepak_init(). It sets the save_type
 * and save_size_bytes fields in the s_gamepak_info struct.
 */
static void _gamepak_detect_and_load_save_media(void) {
    s_gamepak_info.save_type = N64_SAVE_TYPE_NONE;
    s_gamepak_info.save_size_bytes = 0;
    memset(s_save_page_buffer, 0xFF, N64_SAVE_PAGE_BUFFER_SIZE);

    //printf("DEBUG: entering save detection\n");
    if (_gamepak_probe_sram()) {
        //printf("DEBUG: SRAM probe returned true\n");
        s_gamepak_info.save_type = N64_SAVE_TYPE_SRAM;
        s_gamepak_info.save_size_bytes = N64_SRAM_SIZE;
        gamepak_read_sram_bytes(N64_SRAM_BASE, s_save_page_buffer, N64_SAVE_PAGE_BUFFER_SIZE);
        printf("SRAM detected and initialized.\n");
        return;
    }
    //printf("DEBUG: SRAM not found, trying EEPROM\n");
    size_t detected_eeprom_size = joybus_get_eeprom_size();
    //printf("DEBUG: joybus_get_eeprom_size() = %u\n", (unsigned)detected_eeprom_size);
    // --- Probe for EEPROM ---

        if (detected_eeprom_size > 0) {
            
            // Map the physical size back to our enum
            if (detected_eeprom_size == 512) {
                s_gamepak_info.save_type = N64_SAVE_TYPE_EEPROM_4K;
            } else {
                s_gamepak_info.save_type = N64_SAVE_TYPE_EEPROM_16K;
            }
            
            s_gamepak_info.save_size_bytes = detected_eeprom_size;
            
            // Pre-load the first chunk of save data into the buffer
            gamepak_read_eeprom_bytes(0, (uint8_t*)s_save_page_buffer, N64_SAVE_PAGE_BUFFER_SIZE);
            return; // Found it, we're done.
        }

    // --- Probe for FlashRAM ---
    if (gamepak_has_flashram()) {
        s_gamepak_info.save_type       = N64_SAVE_TYPE_FLASHRAM;
        s_gamepak_info.save_size_bytes = N64_FLASHRAM_SIZE;
        gamepak_read_flashram_bytes(0, s_save_page_buffer,
                                    N64_SAVE_PAGE_BUFFER_SIZE);
        return;                 // Found it, we're done.
    }
}

static void _gamepak_refresh_save_page_cache(void) {
    switch (s_gamepak_info.save_type) {
        case N64_SAVE_TYPE_SRAM:
            gamepak_read_sram_bytes(N64_SRAM_BASE, s_save_page_buffer, N64_SAVE_PAGE_BUFFER_SIZE);
            break;
        case N64_SAVE_TYPE_EEPROM_4K:
        case N64_SAVE_TYPE_EEPROM_16K:
            gamepak_read_eeprom_bytes(0, s_save_page_buffer, N64_SAVE_PAGE_BUFFER_SIZE);
            break;
        case N64_SAVE_TYPE_FLASHRAM:
            gamepak_read_flashram_bytes(0, s_save_page_buffer, N64_SAVE_PAGE_BUFFER_SIZE);
            break;
        default:
            break;
    }
}

//==============================================================================
// Initialization and Status
//==============================================================================

bool gamepak_init(void) {
    // 1. Reset all our internal state.
    memset(&s_gamepak_info, 0, sizeof(s_gamepak_info));
    s_gamepak_info.valid = false;

    // --- STEP 1: ADBUS (ROM) INITIALIZATION ---
    // Initialize and use the parallel Adbus FIRST, while the bus is quiet.
    if (!adbus_init()) {
        return false;
    }

    // Reset the cartridge bus.
    // adbus_assert_reset(true);
    // sleep_ms(5);
    // adbus_assert_reset(false);
    // sleep_ms(10);

    // Read the full 64-byte header from the ROM.
    if (!gamepak_read_rom_bytes(N64_ROM_BASE, (uint8_t*)&s_gamepak_info.header, N64_HEADER_SIZE)) {
        // This fails if there's a hardware read issue.
        return false;
    }
    
    // After the read, check if the bus was open (no cart).
    uint32_t first_dword = s_gamepak_info.header.initial_settings;
    if (first_dword == 0xFFFFFFFF || first_dword == 0x00000000) {
        return false; // No cartridge is present.
    }

    s_golden_header_value = first_dword;

    // --- STEP 2: Detect ROM size ---
    // TEMPORARY STUB: Only a handful of known game IDs are hard-coded here for
    // early testing. This is NOT a complete lookup table. Any unrecognized cart
    // falls through to _gamepak_detect_rom_size(), which probes for mirrors and
    // should work correctly for all standard retail carts. Once mirror detection
    // is confirmed reliable across a wider range of carts, the hard-coded IDs
    // below can be removed entirely, or replaced with a proper ID->size table
    // if faster init time becomes a priority.
    char game_id[5];
    memcpy(game_id, s_gamepak_info.header.game_id, 4);
    game_id[4] = '\0';

    if (memcmp(game_id, "NGEE", 4) == 0)
        s_gamepak_info.rom_size_bytes = 12u * 1024 * 1024;       // GoldenEye 007
    else if (memcmp(game_id, "CZLE", 4) == 0 ||
             memcmp(game_id, "NZSE", 4) == 0)
        s_gamepak_info.rom_size_bytes = 32u * 1024 * 1024;       // OoT / Majora's Mask
    else
        s_gamepak_info.rom_size_bytes = _gamepak_detect_rom_size();

    // --- STEP 3: JOYBUS (SAVE) INITIALIZATION ---
    // NOW that we have a valid header and all ROM reading is done, it is safe
    // to initialize the Joybus PIO, which can be electrically noisy.
    if (!joybus_init()) {
        //printf("DEBUG: joybus_init() FAILED\n");
        s_gamepak_info.save_type = N64_SAVE_TYPE_NONE;
    } else {
        //printf("DEBUG: joybus_init() OK\n");
        _gamepak_detect_and_load_save_media();
    }

    // --- STEP 4: FINALIZE ---
    // We have a valid ROM header, so mark the GamePak info as valid.
    s_gamepak_info.valid = true;
    return true;
}

bool gamepak_is_present(void) {
    if (s_golden_header_value == 0) return false;
    
    uint16_t word1 = gamepak_read_rom_word(N64_ROM_BASE);
    uint16_t word2 = gamepak_read_rom_word(N64_ROM_BASE + 2);
    uint32_t current_header_value = ((uint32_t)word1 << 16) | word2;

    return (current_header_value == s_golden_header_value);
}

const n64_gamepak_info_t* gamepak_get_info(void) {
    return s_gamepak_info.valid ? &s_gamepak_info : NULL;
}


//==============================================================================
// Cartridge Information Accessors
//==============================================================================

const n64_gamepak_header_t* gamepak_get_header(void) {
    return s_gamepak_info.valid ? &s_gamepak_info.header : NULL;
}

const uint8_t* gamepak_get_save_page_buffer(void) {
    // Only return a valid pointer if the driver has been successfully initialized.
    if (!s_gamepak_info.valid) {
        return NULL;
    }
    // Return a pointer to our private, static buffer.
    return s_save_page_buffer;
}

void gamepak_get_rom_title(char* buffer, size_t buffer_len) {
    if (!s_gamepak_info.valid || !buffer || buffer_len == 0) {
        if (buffer && buffer_len > 0) buffer[0] = '\0';
        return;
    }

    // Formatting logic for title.
    const size_t title_len = sizeof(s_gamepak_info.header.title);
    
    // Copy a safe amount of the title
    size_t len_to_copy = title_len;
    if (len_to_copy > buffer_len - 1) {
        len_to_copy = buffer_len - 1;
    }
    memcpy(buffer, s_gamepak_info.header.title, len_to_copy);
    
    // Trim trailing spaces from the copied string
    while (len_to_copy > 0 && buffer[len_to_copy - 1] == ' ') {
        --len_to_copy;
    }
    buffer[len_to_copy] = '\0'; // Null-terminate
}

n64_save_type_t gamepak_get_save_type(void) {
    return s_gamepak_info.save_type;
}

size_t gamepak_get_save_size(void) {
    return s_gamepak_info.save_size_bytes;
}

// returns CRC1 (unchanged)
uint32_t gamepak_get_rom_crc1(void) {
    return s_gamepak_info.valid ? s_gamepak_info.header.crc1 : 0;
}

// returns CRC2 (unchanged)
uint32_t gamepak_get_rom_crc2(void) {
    return s_gamepak_info.valid ? s_gamepak_info.header.crc2 : 0;
}

// returns a pointer to the 4-byte game ID (e.g. "CZGE", "NGEE")
// caller must not modify or free this pointer
char *gamepak_get_game_id(void) {
    return s_gamepak_info.valid
         ? s_gamepak_info.header.game_id
         : "";
}

// returns the 1-byte ROM version (at offset 0x3F)
uint8_t gamepak_get_rom_version(void) {
    return s_gamepak_info.valid
         ? s_gamepak_info.header.version
         : 0;
}

//==============================================================================
// ROM Access Functions
//==============================================================================

uint16_t gamepak_read_rom_word(uint32_t rom_address) {
    adbus_latch_address(rom_address);
    return adbus_read_word();
}

// bool gamepak_read_rom_bytes(uint32_t rom_address, uint8_t* buffer, size_t length) {
//     if (!buffer || (length % 2) != 0) return false;

//     for (size_t i = 0; i < length; i += 2) {
//         uint16_t word = gamepak_read_rom_word(rom_address + i);
//         buffer[i]     = (uint8_t)(word >> 8);
//         buffer[i + 1] = (uint8_t)(word & 0xFF);
//     }
//     return true;
// }

bool gamepak_read_rom_bytes(uint32_t rom_address, uint8_t* buffer, size_t length) {
    if (!buffer || (length % 2) != 0) return false;

    // Latch the starting address ONCE before the loop.
    adbus_latch_address(rom_address);

    // Read consecutive words. The cartridge automatically increments its internal address pointer
    // with each read, so we don't need to re-latch the address.
    for (size_t i = 0; i < length; i += 2) {
        uint16_t word = adbus_read_word();
        buffer[i]     = (uint8_t)(word >> 8);
        buffer[i + 1] = (uint8_t)(word & 0xFF);
    }
    
    return true;
}


//==============================================================================
// SRAM Access Functions
//==============================================================================

// Public — just checks what was detected at init time
bool gamepak_has_sram(void) {
    return (s_gamepak_info.save_type == N64_SAVE_TYPE_SRAM);
}

uint16_t gamepak_read_sram_word(uint32_t sram_address) {
    adbus_latch_address(sram_address);
    return adbus_read_word();
}

bool gamepak_write_sram_word(uint32_t sram_address, uint16_t value) {
    adbus_set_direction(true);
    adbus_latch_address(sram_address);
    adbus_write_word(value);
    adbus_set_direction(false);
    return true;
}

bool gamepak_read_sram_bytes(uint32_t sram_address, uint8_t* buffer, size_t length) {
    if (!buffer || (length % 2) != 0) return false;

    for (size_t i = 0; i < length; i += 2) {
        uint16_t word = gamepak_read_sram_word(sram_address + i);
        buffer[i]     = (uint8_t)(word >> 8);
        buffer[i + 1] = (uint8_t)(word & 0xFF);
    }
    return true;
}

bool gamepak_write_sram_bytes(uint32_t sram_address, const uint8_t* buffer, size_t length) {
    if (!buffer || (length % 2) != 0) return false;

    for (size_t i = 0; i < length; i += 2) {
        uint16_t word = ((uint16_t)buffer[i] << 8) | buffer[i + 1];
        if (!gamepak_write_sram_word(sram_address + i, word)) {
            return false;
        }
    }

    _gamepak_refresh_save_page_cache();

    return true;
}


//==============================================================================
// EEPROM Access Functions
//==============================================================================

bool gamepak_has_eeprom(void) {
    // Actually ask the joybus layer if the hardware probe found anything!
    return (joybus_get_eeprom_size() > 0);
}

bool gamepak_read_eeprom_bytes(uint32_t address, uint8_t* buffer, size_t length) {
    if (!buffer || joybus_get_eeprom_size() == 0) return false;
    
    // This higher-level function abstracts the 8-byte block nature of EEPROM reads.
    size_t eeprom_size = joybus_get_eeprom_size();
    if ((address + length) > eeprom_size) return false; // Out of bounds

    uint8_t block_buffer[8];
    size_t current_pos = 0;

    while (current_pos < length) {
        uint32_t current_addr = address + current_pos;
        uint8_t block_index = current_addr / 8;
        uint8_t start_offset_in_block = current_addr % 8;

        if (!joybus_read_eeprom_block(block_index, block_buffer)) {
            return false;
        }

        size_t bytes_to_copy = 8 - start_offset_in_block;
        if (bytes_to_copy > (length - current_pos)) {
            bytes_to_copy = length - current_pos;
        }

        memcpy(buffer + current_pos, block_buffer + start_offset_in_block, bytes_to_copy);
        current_pos += bytes_to_copy;
    }
    return true;
}

bool gamepak_write_and_verify_eeprom_bytes(uint32_t address,
                                           const uint8_t* buffer,
                                           size_t length)
{
    if (!buffer || length == 0) return false;

    size_t eeprom_size = joybus_get_eeprom_size();
    if ((address + length) > eeprom_size) return false;      // out of range

    const uint8_t  *src      = buffer;
    uint32_t        addr_cur = address;
    bool            touched_first_512 = (address < N64_SAVE_PAGE_BUFFER_SIZE);

    while (length)
    {
        uint8_t  block_idx      = addr_cur / 8;
        uint8_t  offset_in_block = addr_cur & 0x7;
        size_t   bytes_this     = 8 - offset_in_block;
        if (bytes_this > length) bytes_this = length;

        /* 1. Read existing 8-byte block */
        uint8_t shadow[8];
        if (!joybus_read_eeprom_block(block_idx, shadow)) return false;

        /* 2. Merge caller’s data into shadow copy */
        memcpy(&shadow[offset_in_block], src, bytes_this);

        /* 3. Write + verify with up-to-3 retries */
        bool ok = false;
        for (int retry = 0; retry < 3 && !ok; ++retry)
        {
            if (!joybus_write_eeprom_block(block_idx, shadow)) continue;

            uint8_t verify[8];
            if (!joybus_read_eeprom_block(block_idx, verify)) continue;
            ok = (memcmp(shadow, verify, 8) == 0);
        }
        if (!ok) return false;          // give up on persistent failure

        /* 4. Advance */
        src      += bytes_this;
        addr_cur += bytes_this;
        length   -= bytes_this;
    }

    /* 5. Keep the 512-byte cache coherent */
    if (touched_first_512)
        _gamepak_refresh_save_page_cache();

    return true;
}



// NOTE: tud_task() removed. Entire FlashRAM write/erase path is being
// rewritten. Do not invest time fixing USB servicing in these functions.
//==============================================================================
// FlashRAM Access Functions (Stubs for Future Expansion)
//==============================================================================

/**
 * @brief  Send a 32-bit command to the FlashRAM command register.
 * Splits into two 16-bit words (high first), latches address, writes, then resets bus direction.
 */
static void gamepak_send_flashram_command(uint32_t cmd) {
    uint16_t low  = (uint16_t)(cmd & 0xFFFFu);
    uint16_t high = (uint16_t)(cmd >> 16);

    adbus_set_direction(true);
    
    // ONLY LATCH ONCE!
    adbus_latch_address(FLASHRAM_CMD_REG);
    adbus_write_word(high);
    adbus_write_word(low);
    
    adbus_set_direction(false);
}

static inline void flashram_set_addr(uint32_t byte_addr) {
    // Strip 0x08000000 off the absolute address
    uint32_t offset = byte_addr - N64_SRAM_BASE; 
    
    // Divide by 128 bytes to get the absolute page number (0 to 1023)
    uint16_t page_idx = (uint16_t)((offset >> 7) & 0xFFFFu); 
    
    gamepak_send_flashram_command(FLASHRAM_ERASE_CMD | page_idx);
}


/**
 * @brief  NEW: Polls the FlashRAM status register until it is ready.
 * @note   This is ESSENTIAL. Operations will fail without waiting for the chip.
 *
 * @return true if the chip became ready, false on timeout.
 */
static bool gamepak_flashram_wait_ready(void) {
    uint32_t timeout = 0;
    // The known idle signature for Majora's Mask (Macronix)
    const uint8_t mx_idle[] = {0x11, 0x11, 0x80, 0x01, 0x00, 0xC2, 0x00, 0x1E};
    
    while (timeout < 1000) { // Up to 1000ms timeout
        gamepak_send_flashram_command(FLASHRAM_SET_STATUS_MODE_CMD); // 0xE1
        sleep_us(50);
        
        adbus_set_direction(true);
        adbus_latch_address(N64_SRAM_BASE);
        adbus_set_direction(false);
        
        bool match = true;
        for (int i = 0; i < 8; i += 2) {
            uint16_t w = adbus_read_word();
            uint8_t hi = (uint8_t)(w >> 8);
            uint8_t lo = (uint8_t)(w & 0xFF);
            if (hi != mx_idle[i] || lo != mx_idle[i+1]) {
                match = false;
            }
        }
        
        if (match) {
            return true; // The chip is answering with its ID, it is IDLE!
        }
        
        sleep_ms(1);
        timeout++;
    }
    return false;
}

bool gamepak_has_flashram(void) {
    // printf("\n--- Running FlashRAM Detection ---\n");

    // 1. Reset the chip to a known state.
    // printf("Step 1: Sending RESET command...\n");
    gamepak_send_flashram_command(FLASHRAM_RESET_CMD);
    if (!gamepak_flashram_wait_ready()) {
        // printf("DEBUG RESULT: FAILED. Wait after initial RESET timed out.\n");
        return false;
    }
    // printf("Step 1: OK.\n");

    // 2. Send the command to enter status register mode.
    // printf("Step 2: Sending SET STATUS MODE command (0xE1000000)...\n");
    gamepak_send_flashram_command(FLASHRAM_SET_STATUS_MODE_CMD);

    // 3. Latch the base address to read the ID values.
    // printf("Step 3: Latching base address (0x%08lX)...\n", N64_SRAM_BASE);
    adbus_latch_address(N64_SRAM_BASE);

    // 4. Read the full 8-byte status/ID block from the chip.
    // printf("Step 4: Reading 8-byte status/ID block...\n");
    uint8_t id_block[8];
    for (int i = 0; i < 8; i += 2) {
        uint16_t word = adbus_read_word();
        id_block[i]     = (uint8_t)(word >> 8);
        id_block[i + 1] = (uint8_t)(word & 0xFF);
    }

    // THIS IS THE MOST IMPORTANT PART: Print what we received.
    // printf("DEBUG RESULT: Received Block: %02X %02X %02X %02X %02X %02X %02X %02X\n",
    //     id_block[0], id_block[1], id_block[2], id_block[3],
    //     id_block[4], id_block[5], id_block[6], id_block[7]);

    // 5. Send READ ARRAY to drop the chip out of Status Mode!
    // (Do NOT use RESET, as the hardware ignores it here)
    gamepak_send_flashram_command(FLASHRAM_READ_ARRAY_CMD); 
    
    // Give it a tiny moment to switch states
    sleep_us(50);

    // 6. Check the manufacturer and device ID against known values.
    // The device ID is the last byte of the block.
    uint8_t device_id = id_block[7];
    // printf("Step 6: Checking Device ID 0x%02X against known types...\n", device_id);

    switch (device_id) {
        case 0x1E: // Macronix MX29L1100
        case 0x1D: // Macronix MX29L1101
        case 0xF1: // Panasonic MN63F81MPN
            // printf("DEBUG RESULT: SUCCESS! Known FlashRAM ID found.\n");
            return true;
        default:
            // printf("DEBUG RESULT: FAILED. Device ID is not recognized.\n");
            return false;
    }
}

bool gamepak_read_flashram_bytes(uint32_t address, uint8_t* buffer, size_t length) {
    if (!buffer || (length % 2) != 0) {
        return false;
    }

    // --- SETUP: Put the chip into Read Array Mode just once ---
    gamepak_send_flashram_command(FLASHRAM_RESET_CMD);
    if (!gamepak_flashram_wait_ready()) {
        return false;
    }
    gamepak_send_flashram_command(FLASHRAM_READ_ARRAY_CMD);

    // --- READ LOOP: Process the data in chunks ---
    size_t bytes_read = 0;
    while (bytes_read < length) {
        size_t chunk_size = 128;
        if (bytes_read + chunk_size > length) {
            chunk_size = length - bytes_read;
        }

        // THE FIX: Translate the byte address to the chip's word address.
        // This is the specific quirk for the Macronix (0x1E) FlashRAM.
        uint32_t physical_address = (address + bytes_read) >> 1;

        // Latch the translated address for the CURRENT chunk.
        adbus_latch_address(N64_SRAM_BASE + physical_address);

        // Perform a fast, contiguous read of this single chunk.
        for (size_t i = 0; i < chunk_size; i += 2) {
            uint16_t word = adbus_read_word();
            buffer[bytes_read + i]     = (uint8_t)(word >> 8);
            buffer[bytes_read + i + 1] = (uint8_t)(word & 0xFFu);
        }

        bytes_read += chunk_size;
    }

    // --- CLEANUP ---
    gamepak_send_flashram_command(FLASHRAM_RESET_CMD);

    return true;
}

/* --------------------------------------------------------------------
   FlashRAM arbitrary-byte writer (read-modify-rewrite of 128-KB block)
   ------------------------------------------------------------------ */
bool gamepak_write_flashram_bytes(uint32_t addr,
                                         const uint8_t *src,
                                         size_t len)
{
    if (!src || !len) return false;
    if ((addr + len) > N64_FLASHRAM_SIZE) return false;

    /* 1. identify 128-KB block that covers [addr, addr+len) */
    uint32_t block_base = addr & ~(FLASHRAM_BLOCK_SIZE - 1);

    // static uint8_t *blk = NULL;
    //     if (!blk) blk = malloc(FLASHRAM_BLOCK_SIZE);
    //     if (!blk) return false;
    // Static buffer: 128KB fixed allocation — intentional and visible in the map file.
    // With FLASHRAM_BLOCK_SIZE = 128KB and total RP2040 RAM = 264KB, this is a
    // significant but necessary cost for read-modify-write operations on FlashRAM.             < Claude code suggestion, review?
    static uint8_t blk[FLASHRAM_BLOCK_SIZE];

    /* 3. read existing block */
    if (!gamepak_read_flashram_bytes(block_base, blk, FLASHRAM_BLOCK_SIZE))
        return false;

    /* 4. splice caller’s buffer */
    memcpy(blk + (addr - block_base), src, len);

    /* 5. erase + program whole block */
    if (!flashram_erase_block(block_base)) return false;
    for (uint32_t off = 0; off < FLASHRAM_BLOCK_SIZE; off += FLASHRAM_PAGE_SIZE)
        if (!flashram_program_page(block_base + off, blk + off))
            return false;

    /* 6. refresh first-512-B cache if touched */
    if (block_base == 0)
        _gamepak_refresh_save_page_cache();

    return true;
}


bool gamepak_write_flashram_sector(uint32_t address, const uint8_t *buffer)
{
    if (!buffer || (address % FLASHRAM_BLOCK_SIZE) != 0)
        return false;                               /* must be 128-KB aligned */

    if (!flashram_erase_block(address))             /* 1. erase */
        return false;

    for (uint32_t off = 0; off < FLASHRAM_BLOCK_SIZE; off += FLASHRAM_PAGE_SIZE)
        if (!flashram_program_page(address + off, buffer + off))
            return false;                           /* 2. program + verify */

    if (address == 0)                               /* 3. refresh cache */
        _gamepak_refresh_save_page_cache();

    return true;
}

bool flashram_program_page(uint32_t byte_addr, const uint8_t data[FLASHRAM_PAGE_SIZE]) {
    // 1. Force IDLE state
    gamepak_send_flashram_command(FLASHRAM_SET_STATUS_MODE_CMD); 
    gamepak_send_flashram_command(FLASHRAM_READ_ARRAY_CMD); // 0xF0
    sleep_us(50);
    
    // 2. Setup Addressing - DO NOT MASK WITH 0x7F
    // Macronix needs the absolute page index (0 to 1023)
    uint32_t offset = byte_addr - N64_SRAM_BASE;
    uint16_t absolute_page_idx = (uint16_t)(offset >> 7); 

    // 3. Set Bank and Program Mode
    flashram_set_addr(byte_addr); 
    gamepak_send_flashram_command(FLASHRAM_PROGRAM_CMD); // 0xB4
    sleep_us(10); // Sanni uses a tiny gap here for the FIFO to prep

    // 4. Data Transfer
    adbus_set_direction(true);
    for (int i = 0; i < FLASHRAM_PAGE_SIZE; i += 2) {
        // Latching every word is slow, but prevents FIFO desync
        adbus_latch_address(N64_SRAM_BASE + i);
        adbus_write_word(((uint16_t)data[i] << 8) | data[i + 1]);
        __asm volatile("nop\nnop\nnop\nnop\n"); 
    }
    adbus_set_direction(false);

    // 5. Commit the Write
    // Use the ABSOLUTE page index here
    gamepak_send_flashram_command(FLASHRAM_PROGRAM_OFFSET_CMD | absolute_page_idx);
    gamepak_send_flashram_command(FLASHRAM_EXECUTE_CMD); // 0xD2

    // 6. Wait for Hardware
    sleep_us(500);
    if (!gamepak_flashram_wait_ready()) return false;

    // 7. Cleanup & Verify
    gamepak_send_flashram_command(FLASHRAM_READ_ARRAY_CMD); // Exit Status Mode
    sleep_us(50);

    uint8_t verify[FLASHRAM_PAGE_SIZE];
    if (!gamepak_read_flashram_bytes(byte_addr, verify, FLASHRAM_PAGE_SIZE)) return false;
    
    return memcmp(data, verify, FLASHRAM_PAGE_SIZE) == 0;
}

//     return false; // Timed out
// }
bool flashram_erase_block(uint32_t byte_addr) {
    (void)byte_addr; 

    // 1. Initial Reset/Execute sequence to clear state
    gamepak_send_flashram_command(FLASHRAM_EXECUTE_CMD); // EXECUTE
    sleep_ms(10);
    gamepak_send_flashram_command(FLASHRAM_SET_STATUS_MODE_CMD); // STATUS
    sleep_ms(10);

    // 2. Erase each 16KB bank individually
    for (uint32_t bank = 0; bank < 8; bank++) {
        // --- THE FIX: MACRONIX BANK ERASE COMMAND ---
        // Macronix expects the bank index at bits 20-22, ORed with 0x4B
        // Command becomes: 0x4B000000 | (bank << 20) | 0x3FFF
        // Wait: Sanni uses 0x4B000000 | (bank << 7) | 0x7F
        
        // Let's use the absolute Sanni-verified command for Macronix:
        uint32_t bank_cmd = FLASHRAM_ERASE_CMD | ((bank << 7) | 0x7F);
        
        gamepak_send_flashram_command(bank_cmd);
        sleep_ms(1);
        
        gamepak_send_flashram_command(FLASHRAM_ERASE_MODE_CMD); // ERASE MODE
        sleep_ms(1);
        
        gamepak_send_flashram_command(FLASHRAM_EXECUTE_CMD); // EXECUTE
        
        if (!gamepak_flashram_wait_ready()) {
            return false;
        }
    }
    
    gamepak_send_flashram_command(FLASHRAM_READ_ARRAY_CMD); // READ ARRAY
    return true;
}

void gamepak_bus_warmup(void) {
    // Force the bus into a known state by driving it LOW 
    // and cycling the latches once without a chip select.
    adbus_set_direction(true);
    sio_hw->gpio_clr = N64_ADBUS_GPIO_MASK; 
    adbus_latch_address(N64_SRAM_BASE);
    adbus_set_direction(false);
}