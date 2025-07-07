#include "tusb.h"
#include <string.h>
#include <stdlib.h>

// Required project headers
#include "n64/devices/gamepak.h"
#include "usb/filesystem.h"

// Global buffer for the info file content, defined elsewhere
extern char g_info_file_buffer[512];

uint8_t  *flash_cache          = NULL;          /* malloc-once           */
uint8_t   flash_sector_map[FLASHRAM_SECTOR_CNT/8] = {0}; /* 256-bit bitmap */
uint32_t  flash_sectors_seen   = 0;             /* how many bits set     */
bool      flash_write_pending  = false;         /* main-loop flag        */

// --- MSC Inquiry, Test Unit Ready, and Capacity Callbacks (Unchanged) ---

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;
    memcpy(vendor_id, "Pico", 4);
    memcpy(product_id, "pico-pak", 8);
    memcpy(product_rev, "1.0", 3);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    return true; // Drive is always ready
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void)lun;
    *block_size = 512;
    *block_count = fs_get_total_block_count();
}

// --- MSC Read Callback (UPDATED) ---

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    (void)lun;

    const n64_gamepak_info_t* cart_info = gamepak_get_info();
    uint32_t lba_file1_start = fs_get_lba_file1_start(); // ROM
    uint32_t lba_file2_start = fs_get_lba_file2_start(); // Save
    uint32_t lba_file3_start = fs_get_lba_file3_start(); // Info

    // --- Filesystem Metadata & ROM Reading (Unchanged) ---
    if (lba < fs_get_metadata_block_count()) {
        memcpy(buffer, fs_get_block_ptr(lba) + offset, bufsize);
    } else if (lba < lba_file2_start) {
        uint32_t rom_address = N64_ROM_BASE + ((lba - lba_file1_start) * 512) + offset;
        gamepak_read_rom_bytes(rom_address, buffer, bufsize);
    }
    // --- Save File Reading (UPDATED with dynamic dispatch) ---
    else if (lba < lba_file3_start) {
        uint32_t save_address = ((lba - lba_file2_start) * 512) + offset;

        // ============================ THE FIX IS HERE ============================
        switch (cart_info->save_type) {
            case N64_SAVE_TYPE_SRAM:
                gamepak_read_sram_bytes(N64_SRAM_BASE + save_address, buffer, bufsize);
                break;
            case N64_SAVE_TYPE_EEPROM_4K:
            case N64_SAVE_TYPE_EEPROM_16K:
                gamepak_read_eeprom_bytes(save_address, buffer, bufsize);
                break;
            case N64_SAVE_TYPE_FLASHRAM:
                gamepak_read_flashram_bytes(save_address, buffer, bufsize);
                break;
            default:
                // No save media or unknown; return an empty buffer.
                memset(buffer, 0, bufsize);
                break;
        }
        // =======================================================================
    }
    // --- Info File Reading (Unchanged) ---
    else {
        uint8_t block_buffer[512] = {0};
        memcpy(block_buffer, g_info_file_buffer, strlen(g_info_file_buffer));
        memcpy(buffer, block_buffer, bufsize);
    }

    return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t off,
                         uint8_t *buf, uint32_t size)
{
    (void) lun;

    const n64_gamepak_info_t *inf = gamepak_get_info();
    uint32_t save_start = fs_get_lba_file2_start();
    uint32_t info_start = fs_get_lba_file3_start();

    // Ignore writes to the metadata and the read-only INFO.TXT file.
    if (lba < save_start || lba >= info_start) {
        return (int32_t) size;
    }

    // Calculate the byte address within the save file.
    uint32_t addr = ((lba - save_start) * 512u) + off;

    // Dispatch to the correct save-writing function.
    switch (inf->save_type)
    {
        case N64_SAVE_TYPE_SRAM:
            gamepak_write_sram_bytes(addr, buf, size);
            break;

        case N64_SAVE_TYPE_EEPROM_4K:
        case N64_SAVE_TYPE_EEPROM_16K:
            gamepak_write_and_verify_eeprom_bytes(addr, buf, size);
            break;

        case N64_SAVE_TYPE_FLASHRAM:
            // gamepak_write_flashram_bytes(addr, buf, size);
            break;
        default: break;
    }

    // Return the number of bytes processed to indicate success.
    return (int32_t) size;
}

// --- MSC SCSI Callback (Unchanged) ---

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
    (void)lun; (void)scsi_cmd; (void)buffer; (void)bufsize;
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}