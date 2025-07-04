/**
 * @file     gamepak.c
 * @brief    Implementation of the GamePak (cartridge) API.
 * @version  2.0
 */

#include "n64/devices/gamepak.h" // Public API header
#include "n64/bus/adbus.h"      // Low-level hardware access
#include "n64/bus/joybus.h"     // For save game access

#include "pico/stdlib.h"        // For sleep_ms, etc.
#include "tusb.h"

#include <string.h>             // For memset, memcpy
#include <stdio.h>              // for printf() ///## DEBUG ##///
#include <stdlib.h>


//==============================================================================
// Private Module-Level State
//==============================================================================

static n64_gamepak_info_t s_gamepak_info;
static uint8_t s_save_page_buffer[N64_SAVE_PAGE_BUFFER_SIZE];
static uint32_t s_golden_header_value = 0; // For hot-swap detection
 

//==============================================================================
// Private Helper Functions
//==============================================================================

static void gamepak_send_flashram_command(uint32_t cmd);

bool gamepak_read_rom_bytes(uint32_t rom_address, uint8_t* buffer, size_t length);
static bool flashram_erase_block(uint32_t byte_addr);
static bool flashram_program_page(uint32_t byte_addr, const uint8_t data[FLASHRAM_PAGE_SIZE]);

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
    // We need to get the size detected by your original InitEeprom function.
    extern uint32_t gEepromSize;

//    printf("--- Save Detection Log ---\n");

    // Default to no save type
    s_gamepak_info.save_type = N64_SAVE_TYPE_NONE;
    s_gamepak_info.save_size_bytes = 0;
    memset(s_save_page_buffer, 0, N64_SAVE_PAGE_BUFFER_SIZE);
//    printf("State: Initialized to SAVE_TYPE_NONE.\n");

    // --- Probe for SRAM ---
//    printf("Probe: Checking for SRAM...\n");
    if (gamepak_has_sram()) {
//        printf("Probe Result: SRAM Detected.\n");
        s_gamepak_info.save_type = N64_SAVE_TYPE_SRAM;
        s_gamepak_info.save_size_bytes = N64_SRAM_SIZE;
//        printf("State: Set to SAVE_TYPE_SRAM (%zu bytes).\n", s_gamepak_info.save_size_bytes);
        gamepak_read_sram_bytes(N64_SRAM_BASE, s_save_page_buffer, N64_SAVE_PAGE_BUFFER_SIZE);
//        printf("--- Save Detection Finished ---\n");
        return; // Found it, we're done
    }
//    printf("Probe Result: No SRAM found.\n");


    // --- Probe for EEPROM ---
    // We check the global variable that your trusted InitEeprom function sets.
//    printf("Probe: Checking for EEPROM (gEepromSize = %u)...\n", gEepromSize);
    if (gEepromSize > 0) {
//        printf("Probe Result: EEPROM Detected.\n");
        s_gamepak_info.save_size_bytes = gEepromSize;

        if (s_gamepak_info.save_size_bytes == N64_EEPROM_16K_SIZE) {
            s_gamepak_info.save_type = N64_SAVE_TYPE_EEPROM_16K;
        } else {
            s_gamepak_info.save_type = N64_SAVE_TYPE_EEPROM_4K;
        }
        
//        printf("State: Set to %d (%zu bytes).\n", s_gamepak_info.save_type, s_gamepak_info.save_size_bytes);

        // Attempt to read the first page of the save file.
        if (gamepak_read_eeprom_bytes(0, s_save_page_buffer, N64_SAVE_PAGE_BUFFER_SIZE)) {
//            printf("State: Successfully read first 512 bytes into save cache.\n");
        } else {
//            printf("State: ERROR reading first 512 bytes from EEPROM.\n");
        }
//        printf("--- Save Detection Finished ---\n");
        return;
    }
//    printf("Probe Result: No EEPROM found.\n");


    // --- Probe for FlashRAM ---
//    printf("Probe: Checking for FlashRAM...\n");
    if (gamepak_has_flashram()) {
//        printf("Probe Result: FlashRAM Detected.\n");
        s_gamepak_info.save_type       = N64_SAVE_TYPE_FLASHRAM;
        s_gamepak_info.save_size_bytes = N64_FLASHRAM_SIZE;
//        printf("State: Set to SAVE_TYPE_FLASHRAM (%zu bytes).\n", s_gamepak_info.save_size_bytes);

        // Read the first 512-byte “page” into our cache
        // FIX: The address is an offset, so use 0 to read from the beginning.
        if (gamepak_read_flashram_bytes(0, s_save_page_buffer, N64_SAVE_PAGE_BUFFER_SIZE)) {
//            printf("State: Successfully read first 512 bytes into save cache.\n");
        } else {
//            printf("State: ERROR reading first 512 bytes from FlashRAM.\n");
        }

        // printf("--- Save Detection Finished ---\n");
        return;
    }
    // printf("Probe Result: No FlashRAM found.\n");
    // printf("--- Save Detection Finished ---\n");
}

static void _gamepak_refresh_save_page_cache(void) {
    // Check the detected save type and call the appropriate read function.
    switch (s_gamepak_info.save_type) {
        case N64_SAVE_TYPE_SRAM:
            gamepak_read_sram_bytes(N64_SRAM_BASE, s_save_page_buffer, N64_SAVE_PAGE_BUFFER_SIZE);
            break;
        
        case N64_SAVE_TYPE_EEPROM_4K:
        case N64_SAVE_TYPE_EEPROM_16K:
            // The address is 0 for EEPROM reads, and we read the first page.
            gamepak_read_eeprom_bytes(0, s_save_page_buffer, N64_SAVE_PAGE_BUFFER_SIZE);
            break;

        // Add cases for FlashRAM etc. here in the future
        
        default:
            // No detectable/writable save type, do nothing.
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
    adbus_assert_reset(true);
    sleep_ms(5);
    adbus_assert_reset(false);
    sleep_ms(10);

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

    // --- NEW: Detect and store ROM Size ---
    s_gamepak_info.rom_size_bytes = _gamepak_detect_rom_size();

    // --- STEP 2: JOYBUS (SAVE) INITIALIZATION ---
    // NOW that we have a valid header and all ROM reading is done, it is safe
    // to initialize the Joybus PIO, which can be electrically noisy.
    if (!joybus_init()) {
        // If joybus fails, that's okay. We still have a valid ROM, just no save info.
        s_gamepak_info.save_type = N64_SAVE_TYPE_NONE;
    } else {
        // Joybus is up, now we can safely probe for save media.
        _gamepak_detect_and_load_save_media();
    }

    // --- STEP 3: FINALIZE ---
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

bool gamepak_has_sram(void) {
    const uint32_t base = N64_SRAM_BASE;
    const uint32_t test_addr = base + 0x100;
    const uint16_t magic = 0x5A5A;

    if (gamepak_read_sram_word(base) == 0xFFFF) {
        return false;
    }

    uint16_t orig = gamepak_read_sram_word(test_addr);
    gamepak_write_sram_word(test_addr, magic);
    uint16_t readback = gamepak_read_sram_word(test_addr);
    gamepak_write_sram_word(test_addr, orig);

    return (readback == magic);
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

    // Drive bus for write
    adbus_set_direction(true);
    // Select command register
    adbus_latch_address(FLASHRAM_CMD_REG);
    // Send high half, then low half
    adbus_write_word(high);
    adbus_write_word(low);
    // Release bus for read
    adbus_set_direction(false);
}

/**
 * @brief  NEW: Polls the FlashRAM status register until it is ready.
 * @note   This is ESSENTIAL. Operations will fail without waiting for the chip.
 *
 * @return true if the chip became ready, false on timeout.
 */
const uint8_t FLASHRAM_IDLE_STATUS[8] = { 0x11, 0x11, 0x80, 0x01, 0x00, 0xC2, 0x00, 0x1E };
static bool gamepak_flashram_wait_ready(void) {
    // A buffer to hold the 8-byte status read from the chip
    uint8_t current_status[8];

    // Set a generous timeout to avoid infinite loops
    for (int i = 0; i < 500; i++) {
        // 1. Tell the chip to enter status register mode.
        gamepak_send_flashram_command(FLASHRAM_SET_STATUS_MODE_CMD);

        // 2. Latch the base address to read the status register itself.
        adbus_latch_address(N64_SRAM_BASE);

        // 3. Read the full 8-byte status block.
        for (int j = 0; j < 8; j += 2) {
            uint16_t word = adbus_read_word();
            current_status[j]     = (uint8_t)(word >> 8);
            current_status[j + 1] = (uint8_t)(word & 0xFF);
        }

        // 4. Compare the entire block to the known "idle" state.
        if (!memcmp(current_status, FLASH_IDLE_MX1100, 8) ||
            !memcmp(current_status, FLASH_IDLE_MX1101, 8) ||
            !memcmp(current_status, FLASH_IDLE_MN63F81, 8))
            return true;

        // 5. CRITICAL: Wait before trying again.
        sleep_ms(1);
    }

    return false; // Timeout occurred
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

    // 5. Send a final reset to leave the chip in a clean state.
    gamepak_send_flashram_command(FLASHRAM_RESET_CMD);

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

    /* 2. allocate scratch (once) */
    static uint8_t *blk = NULL;
    if (!blk) blk = malloc(FLASHRAM_BLOCK_SIZE);
    if (!blk) return false;

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

/* Erase the 128-KB block that contains <byte_addr> */
static bool flashram_erase_block(uint32_t byte_addr)
{
    uint32_t phys = byte_addr >> 1;                 /* byte → word address */

    tud_task();                                     /* service USB once   */
    gamepak_send_flashram_command(FLASHRAM_ERASE_CMD | phys);
    gamepak_send_flashram_command(FLASHRAM_ERASE_MODE_CMD);  /* 0x7800…   */
    gamepak_send_flashram_command(FLASHRAM_EXECUTE_CMD);

    /* wait-ready loop already calls tud_task() internally */
    return gamepak_flashram_wait_ready();
}

/* Program one 128-B page (address must be page-aligned) */
static bool flashram_program_page(uint32_t byte_addr,
                                  const uint8_t data[FLASHRAM_PAGE_SIZE])
{
    /* Page index within the current 128-KB block (0–1023) */
    uint16_t page_idx = ((byte_addr >> 7) & 0x03FF);   /* 128-B pages */

    /* 1. Enter PROGRAM mode (no address bits) ---------------------------- */
    tud_task();                                       /* USB heartbeat   */
    gamepak_send_flashram_command(FLASHRAM_PROGRAM_CMD);
    sleep_us(20);                                     /* 20 µs guard     */

    /* 2. Burst 128 B to base address 0x0800_0000 ------------------------- */
    adbus_set_direction(true);
    adbus_latch_address(N64_SRAM_BASE);
    for (int i = 0; i < FLASHRAM_PAGE_SIZE; i += 2) {
        uint16_t w = ((uint16_t)data[i] << 8) | data[i+1];
        adbus_write_word(w);
    }
    adbus_set_direction(false);

    /* 3. Select page via A5xxxxxxxx register ----------------------------- */
    gamepak_send_flashram_command(FLASHRAM_PROGRAM_OFFSET_CMD | page_idx);
    sleep_us(20);                                     /* 20 µs guard     */

    /* 4. Execute and poll until chip is ready ---------------------------- */
    gamepak_send_flashram_command(FLASHRAM_EXECUTE_CMD);

    while (!gamepak_flashram_wait_ready()) {
        tud_task();                                   /* keep USB alive  */
    }

    /* 5. Verify ---------------------------------------------------------- */
    uint8_t verify[FLASHRAM_PAGE_SIZE];
    if (!gamepak_read_flashram_bytes(byte_addr, verify, FLASHRAM_PAGE_SIZE))
        return false;

    return memcmp(data, verify, FLASHRAM_PAGE_SIZE) == 0;
}