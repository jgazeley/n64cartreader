#ifndef N64_GAMESHARK_H
#define N64_GAMESHARK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t  present;
    uint8_t  chip_family;
    uint16_t flash_id;
    uint32_t base_addr;
    uint32_t size_bytes;
    uint16_t caps;
    uint16_t flags;
    uint16_t mfg_id;
} n64_gs_info_t;

enum {
    N64_GS_CAP_EXPORT = 1u << 0,
    N64_GS_CAP_IMPORT = 1u << 1,
    N64_GS_CAP_ERASE  = 1u << 2,
};

enum {
    N64_GS_FLAG_ID_WORDS_SWAPPED = 1u << 0,
    N64_GS_FLAG_OPEN_BUS_ID      = 1u << 1,
    N64_GS_FLAG_UNKNOWN_DEVICE   = 1u << 2,
};

/** @brief Unlocks GameShark flash for reading/writing */
void gamepak_gs_unlock(void);

/** @brief Detects known GameShark flash and fills `out_info` */
bool gamepak_gs_probe(n64_gs_info_t *out_info);

/** @brief Reads bytes from GameShark flash */
bool gamepak_gs_read_bytes(uint32_t offset, uint8_t* buffer, size_t length);

/** @brief Erases GameShark flash */
bool gamepak_gs_erase(uint16_t flash_id);

/** @brief Writes bytes to GameShark flash */
bool gamepak_gs_write_bytes(uint16_t flash_id, uint32_t offset, const uint8_t* buffer, size_t length);

/** @brief Resets detected GameShark flash back to normal read mode */
void gamepak_gs_reset_to_read_mode(uint16_t flash_id);

#endif // N64_GAMESHARK_H
