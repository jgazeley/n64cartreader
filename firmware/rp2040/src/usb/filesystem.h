#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "n64/devices/gamepak.h"

// The function now takes all necessary dynamic info
void fs_create_virtual_disk(const n64_gamepak_info_t* info, const char* info_str, size_t info_len);

// Helper functions to get layout info
uint8_t* fs_get_block_ptr(uint32_t lba);
uint32_t fs_get_metadata_block_count(void);
uint32_t fs_get_total_block_count(void);
uint32_t fs_get_lba_file1_start(void); // ROM
uint32_t fs_get_lba_file2_start(void); // SAV
uint32_t fs_get_lba_file3_start(void); // INFO

#endif // FILESYSTEM_H