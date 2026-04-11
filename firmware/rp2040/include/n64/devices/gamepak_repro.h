/**
 * @file     gamepak_repro.h
 * @brief    ROM-space NOR helpers for repro cartridges.
 */
#ifndef N64_GAMEPAK_REPRO_H
#define N64_GAMEPAK_REPRO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t mfg_id;
    uint16_t dev_id0;
    uint16_t dev_id1;
    uint16_t dev_id2;
} n64_rom_flash_id_t;

typedef struct {
    uint16_t normal[32];
    uint16_t query[32];
} n64_rom_flash_diag_t;

enum {
    N64_REPRO_ALIAS_DIAG_WORD_0X000000 = 0,
    N64_REPRO_ALIAS_DIAG_WORD_0X000002 = 1,
    N64_REPRO_ALIAS_DIAG_WORD_0X000800 = 2,
    N64_REPRO_ALIAS_DIAG_WORD_0X001000 = 3,
    N64_REPRO_ALIAS_DIAG_WORD_0X002000 = 4,
    N64_REPRO_ALIAS_DIAG_WORD_0X004000 = 5,
    N64_REPRO_ALIAS_DIAG_WORD_0X008000 = 6,
    N64_REPRO_ALIAS_DIAG_WORD_0X010000 = 7,
    N64_REPRO_ALIAS_DIAG_WORD_0X020000 = 8,
    N64_REPRO_ALIAS_DIAG_WORD_0X040000 = 9,
    N64_REPRO_ALIAS_DIAG_WORD_0X080000 = 10,
    N64_REPRO_ALIAS_DIAG_WORD_0X100000 = 11,
    N64_REPRO_ALIAS_DIAG_WORD_0X200000 = 12,
    N64_REPRO_ALIAS_DIAG_WORD_COUNT = 13,
};

typedef struct {
    // Sampled words at fixed byte offsets defined by the enum above.
    uint16_t words[N64_REPRO_ALIAS_DIAG_WORD_COUNT];
} n64_repro_alias_diag_t;

typedef struct {
    uint16_t pre_word;
    uint16_t first_sr;
    uint16_t final_sr;
    uint16_t post_word;
    uint16_t poll_count;
    uint16_t ok;
} n64_rom_flash_erase_result_t;

/**
 * @brief Issue one AMD/Fujitsu software-reset write (0xF0) in ROM space.
 *
 * Caller must manage bus direction and settle timing around this pulse.
 */
void gamepak_repro_flash_reset(void);

/**
 * @brief Force a repro NOR cart back into read-array mode.
 *
 * This is meant for programmable flash carts that can be left in an ID/query
 * mode by prior diagnostics or programming traffic. Retail carts should not
 * use this as their default read path.
 */
void gamepak_repro_prepare_read_array(void);

/**
 * @brief Read ROM bytes through the guarded repro backend.
 *
 * This keeps the special reset carrier and bulk read sequence isolated from
 * retail carts while letting gamepak.c present a single public ROM API.
 */
bool gamepak_repro_read_rom_bytes(uint32_t rom_address, uint8_t *buffer, size_t length);

/**
 * @brief Read a 512-byte MX29LV640 ROM window using the proven raw stream path.
 *
 * This is specific to the 8 MiB single-chip Macronix repro board family and
 * intentionally bypasses the generic retail ROM reader.
 */
bool gamepak_mx29lv640_stream_read_window(uint32_t rom_address, uint8_t *buffer, uint16_t length);

/**
 * @brief Read MX29LV640 ROM bytes through the lighter-weight steady-state path.
 *
 * Unlike the probe/window helper above, this uses a short read-array reset
 * suitable for repeated chunked reads during normal ROM export.
 */
bool gamepak_mx29lv640_read_rom_bytes(uint32_t rom_address, uint8_t *buffer, size_t length);

bool gamepak_rom_flash_probe(n64_rom_flash_id_t *out);
bool gamepak_rom_flash_probe_diag(n64_rom_flash_diag_t *diag);
bool gamepak_rom_flash_write_buffer(uint32_t addr, const uint8_t *data, uint8_t len);
void gamepak_repro_alias_diag_read(n64_repro_alias_diag_t *diag);
bool gamepak_rom_flash_chip_erase(n64_rom_flash_erase_result_t *diag);

#endif // N64_GAMEPAK_REPRO_H
