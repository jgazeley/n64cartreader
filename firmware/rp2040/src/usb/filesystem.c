#include "usb/filesystem.h"
#include "n64/devices/gamepak.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define DISK_BLOCK_SIZE 512
#define MAX_METADATA_BLOCKS 128
static uint8_t msc_disk[MAX_METADATA_BLOCKS][DISK_BLOCK_SIZE];

// --- Calculated Layout Variables ---
static uint32_t LBA_ROOT_DIR_START, METADATA_BLOCKS, TOTAL_BLOCKS;
static uint32_t LBA_FILE1_START, LBA_FILE2_START, LBA_FILE3_START;

// --- FIX #2: Make this file aware of the global buffer from main.c ---
extern char g_info_file_buffer[512];

// --- Helper Functions ---
static void write_uint16_le(uint8_t* b, uint16_t v) { b[0]=v&0xFF; b[1]=(v>>8)&0xFF; }
static void write_uint32_le(uint8_t* b, uint32_t v) { b[0]=v&0xFF; b[1]=(v>>8)&0xFF; b[2]=(v>>16)&0xFF; b[3]=(v>>24)&0xFF; }

static void format_8_3_filename(const char* basename, const char* ext, uint8_t* dest) {
    memset(dest, ' ', 11);
    for (int i = 0; i < 8 && basename[i] != '\0'; i++) dest[i] = toupper((unsigned char)basename[i]);
    for (int i = 0; i < 3 && ext[i] != '\0'; i++) dest[8 + i] = toupper((unsigned char)ext[i]);
}

// --- FIX #1: Update the function signature to match the header file ---
void fs_create_virtual_disk(const n64_gamepak_info_t* info, const char* info_str, size_t info_len) {
    memset(msc_disk, 0, sizeof(msc_disk));

    // --- Initial Size Calculations ---
    uint32_t rom_size_bytes = (info && info->valid) ? info->rom_size_bytes : 0;
    uint32_t save_size_bytes = (info && info->valid) ? info->save_size_bytes : 0;
    uint32_t info_size_bytes = info_len; // This remains the actual string length for the directory entry.

    // --- Filesystem Parameters ---
    const uint32_t sectors_per_cluster = 8;
    const uint32_t root_dir_entries = 512;
    const uint32_t root_dir_sectors = (root_dir_entries * 32) / DISK_BLOCK_SIZE;

    // --- Step 1: Calculate how many CLUSTERS each file needs (rounding up) ---
    // First, get the raw sector count for each file's data.
    uint32_t raw_sectors_for_rom = (rom_size_bytes + DISK_BLOCK_SIZE - 1) / DISK_BLOCK_SIZE;
    uint32_t raw_sectors_for_sav = (save_size_bytes + DISK_BLOCK_SIZE - 1) / DISK_BLOCK_SIZE;
    uint32_t raw_sectors_for_inf = (info_size_bytes + DISK_BLOCK_SIZE - 1) / DISK_BLOCK_SIZE;

    // Now, determine the number of full clusters needed to store those sectors.
    uint32_t clusters_for_rom = (raw_sectors_for_rom + sectors_per_cluster - 1) / sectors_per_cluster;
    uint32_t clusters_for_sav = (raw_sectors_for_sav + sectors_per_cluster - 1) / sectors_per_cluster;
    uint32_t clusters_for_inf = (raw_sectors_for_inf + sectors_per_cluster - 1) / sectors_per_cluster;

    // ============================ THE FIX IS HERE ============================
    // --- Step 2: Recalculate sector counts based on whole clusters ---
    // This ensures the disk layout reserves enough space for every sector in every allocated cluster.
    uint32_t sectors_for_rom = clusters_for_rom * sectors_per_cluster;
    uint32_t sectors_for_sav = clusters_for_sav * sectors_per_cluster;
    uint32_t sectors_for_inf = clusters_for_inf * sectors_per_cluster; // This will now be 8, not 1.
    // =======================================================================

    // --- Step 3: Lay out the disk using the CORRECTED sector counts ---
    uint32_t total_data_clusters = clusters_for_rom + clusters_for_sav + clusters_for_inf;
    uint32_t sectors_per_fat = ((total_data_clusters + 2) * 2 + DISK_BLOCK_SIZE - 1) / DISK_BLOCK_SIZE;

    uint32_t lba_fat_start = 1;
    LBA_ROOT_DIR_START = lba_fat_start + sectors_per_fat;
    METADATA_BLOCKS = LBA_ROOT_DIR_START + root_dir_sectors;

    // Calculate final file start LBAs and TOTAL disk size using the cluster-aligned sector counts.
    LBA_FILE1_START = METADATA_BLOCKS;
    LBA_FILE2_START = LBA_FILE1_START + sectors_for_rom;
    LBA_FILE3_START = LBA_FILE2_START + sectors_for_sav;

    // The total size now correctly includes the padding sectors.
    TOTAL_BLOCKS = METADATA_BLOCKS + sectors_for_rom + sectors_for_sav + sectors_for_inf;


    // --- Boot Sector, FAT, and Root Directory Creation (This code remains unchanged) ---

    // --- Create Boot Sector ---
    uint8_t* bs = msc_disk[0];
    memcpy(bs, "\xEB\x3C\x90MSDOS5.0", 11);
    write_uint16_le(bs + 11, DISK_BLOCK_SIZE);
    bs[13] = sectors_per_cluster;
    write_uint16_le(bs + 14, lba_fat_start);
    bs[16] = 1;
    write_uint16_le(bs + 17, root_dir_entries);
    if (TOTAL_BLOCKS < 0x10000) write_uint16_le(bs + 19, TOTAL_BLOCKS); else write_uint16_le(bs + 19, 0);
    bs[21] = 0xF8;
    write_uint16_le(bs + 22, sectors_per_fat);
    if (TOTAL_BLOCKS >= 0x10000) write_uint32_le(bs + 32, TOTAL_BLOCKS); else write_uint32_le(bs + 32, 0);
    bs[38] = 0x29;
    write_uint32_le(bs + 39, 0x12345678);
    memcpy(bs + 43, "N64 DRIVE  ", 11);
    memcpy(bs + 54, "FAT16   ", 8);
    bs[510] = 0x55; bs[511] = 0xAA;

    // --- Create FAT ---
    uint16_t* fat = (uint16_t*)msc_disk[lba_fat_start];
    fat[0] = 0xFFF8; fat[1] = 0xFFFF;
    uint16_t current_cluster = 2;
    for (uint32_t i = 0; i < clusters_for_rom; i++) fat[current_cluster + i] = (i == (clusters_for_rom - 1)) ? 0xFFFF : (current_cluster + i + 1);
    current_cluster += clusters_for_rom;
    for (uint32_t i = 0; i < clusters_for_sav; i++) fat[current_cluster + i] = (i == (clusters_for_sav - 1)) ? 0xFFFF : (current_cluster + i + 1);
    current_cluster += clusters_for_sav;
    for (uint32_t i = 0; i < clusters_for_inf; i++) fat[current_cluster + i] = (i == (clusters_for_inf - 1)) ? 0xFFFF : (current_cluster + i + 1);

    // --- Create Root Directory ---
    uint8_t* root_dir = msc_disk[LBA_ROOT_DIR_START];
    char basename[9] = {0};
    if (info && info->valid) { memcpy(basename, info->header.game_id, 4); } else { strcpy(basename, "NOCART"); }

    uint8_t* entry = root_dir;
    if (rom_size_bytes > 0) {
        format_8_3_filename(basename, "N64", entry);
        entry[11] = 0x21;
        write_uint16_le(entry + 26, 2);
        write_uint32_le(entry + 28, rom_size_bytes);
        entry += 32;
    }
    if (save_size_bytes > 0) {
        format_8_3_filename(basename, "SAV", entry);
        entry[11] = 0x20;
        write_uint16_le(entry + 26, 2 + clusters_for_rom);
        write_uint32_le(entry + 28, save_size_bytes);
        entry += 32;
    }
    format_8_3_filename("CartInfo", "txt", entry);
    entry[11] = 0x21;
    write_uint16_le(entry + 26, 2 + clusters_for_rom + clusters_for_sav);
    // The file SIZE is still the actual text length. The space it OCCUPIES is a full cluster.
    write_uint32_le(entry + 28, info_size_bytes);

    // --- Add this printf for verification ---
    printf("TOTAL_BLOCKS=%lu  INFO start LBA=%lu  INFO spans %lu sectors\n",
       TOTAL_BLOCKS, LBA_FILE3_START, sectors_for_inf);
}

// --- Getter Functions ---
uint8_t* fs_get_block_ptr(uint32_t lba) { return (lba < METADATA_BLOCKS) ? msc_disk[lba] : NULL; }
uint32_t fs_get_metadata_block_count(void) { return METADATA_BLOCKS; }
uint32_t fs_get_total_block_count(void) { return TOTAL_BLOCKS; }
uint32_t fs_get_lba_file1_start(void) { return LBA_FILE1_START; }
uint32_t fs_get_lba_file2_start(void) { return LBA_FILE2_START; }
uint32_t fs_get_lba_file3_start(void) { return LBA_FILE3_START; }