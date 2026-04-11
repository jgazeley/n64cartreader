#include "headless/protocol.h"

#include "utils/byteswap.h"
#include "utils/crc.h"
#include "utils/packet.h"
#include "utils/transport.h"
#include "n64/bus/joybus.h"
#include "n64/devices/gamepak.h"
#include "n64/devices/gamepak_repro.h"
#include "n64/devices/gameshark.h"
#include "n64/devices/controller.h"
#include "fw_build_info.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#define TEST_BUF_SIZE 512u
#define FW_VERSION_PAYLOAD_SIZE 64u

// Command frames sent by host.
// Layout (8 bytes):
//   [0..3]  magic 'P','P','K','1'
//   [4]     command id
//   [5]     arg0 (8-bit)
//   [6..7]  arg1 (big-endian 16-bit)
typedef struct __attribute__((packed)) {
    uint8_t magic[4];
    uint8_t cmd;
    uint8_t arg0;
    uint8_t arg1_be[2];
} cmd_frame_t;

// Response frames sent by Pico.
// Layout (8 bytes):
//   [0..3]  magic 'R','P','K','1'
//   [4]     command id echo
//   [5]     status code
//   [6..7]  value (big-endian 16-bit)
typedef struct __attribute__((packed)) {
    uint8_t magic[4];
    uint8_t cmd;
    uint8_t status;
    uint8_t value_be[2];
} rsp_frame_t;

enum {
    CMD_PING       = 0x01,
    CMD_GET_INFO   = 0x02,
    CMD_GET_VERSION = 0x03,
    CMD_FILLBUF    = 0x10,
    CMD_CLEARBUF   = 0x11,
    CMD_WRITEMSG   = 0x12,
    CMD_SWAP16     = 0x13,
    CMD_SWAP32     = 0x14,
    CMD_CRC16      = 0x15,
    CMD_EXPORTBUF  = 0x20,
    CMD_IMPORTBUF  = 0x21,
    CMD_STREAM32K  = 0x30,
    CMD_IMPORT32K  = 0x31,

    // N64 headless command set.
    CMD_N64_RESCAN      = 0x40,
    CMD_N64_STATUS      = 0x41,
    CMD_N64_EXPORT_SAVE = 0x42,
    CMD_N64_IMPORT_SAVE = 0x43,
    CMD_N64_ROM_INFO    = 0x44,
    CMD_N64_EXPORT_ROM  = 0x45,
    CMD_N64_EXPORT_HEADER = 0x46,
    CMD_N64_CONTROLLER_PROBE = 0x47,
    CMD_N64_CONTROLLER_POLL = 0x48,
    CMD_N64_EXPORT_MPK = 0x49,
    CMD_N64_IMPORT_MPK = 0x4A,
    CMD_N64_GS_INFO    = 0x4B,
    CMD_N64_GS_EXPORT  = 0x4C,
    CMD_N64_EXPORT_SRAM_RAW = 0x4D,
    CMD_N64_GS_IMPORT  = 0x4E,
    CMD_N64_SET_SAVE_CFG   = 0x4F,
    CMD_N64_SET_ROM_SIZE   = 0x50,
    CMD_N64_ROM_FLASH_ID   = 0x51,
    CMD_N64_ROM_FLASH_ERASE = 0x52,
    // Legacy opcode name retained for wire compatibility; the payload is now
    // a read-only aliasing diagnostic rather than a write test.
    CMD_N64_ROM_FLASH_WRITE_TEST = 0x53,
    CMD_N64_REPRO_READ_WORD      = 0x54,
    CMD_N64_MX29LV640_EXPORT_WINDOW_STREAM = 0x63,
};

enum {
    ST_OK       = 0x00,
    ST_BAD_CMD  = 0x01,
    ST_IO_ERR   = 0x02,
    ST_BAD_ARG  = 0x03,
};

static uint8_t s_test_buffer[TEST_BUF_SIZE];
static bool s_n64_ready = false;

static inline uint16_t be16_decode(const uint8_t b[2]) {
    return (uint16_t)(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
}

static inline void be16_encode(uint16_t v, uint8_t out[2]) {
    out[0] = (uint8_t)(v >> 8);
    out[1] = (uint8_t)(v & 0xFF);
}

static bool write_bytes_checked(const uint8_t *buf, size_t len) {
    const transport_t *t = transport_get();
    if (!t || !t->write_bytes || !t->flush) {
        return false;
    }
    if (!t->write_bytes(buf, len)) {
        return false;
    }
    t->flush();
    return true;
}

static bool send_response(uint8_t cmd, uint8_t status, uint16_t value) {
    rsp_frame_t rsp = {
        .magic = { 'R', 'P', 'K', '1' },
        .cmd = cmd,
        .status = status,
        .value_be = { 0, 0 },
    };
    be16_encode(value, rsp.value_be);
    return write_bytes_checked((const uint8_t *)&rsp, sizeof(rsp));
}

static bool read_command_frame(cmd_frame_t *out) {
    const transport_t *t = transport_get();
    if (!t || !t->read_with_timeout) {
        return false;
    }

    uint8_t b = 0;
    // Sync to magic byte 0. Scan through up to 1024 bytes of garbage
    // in-place instead of returning on every non-'P' byte (which would
    // take minutes to clear a stale stream at 1 byte per main-loop tick).
    for (int scan = 0; scan < 1024; ++scan) {
        if (!t->read_with_timeout(&b, 20)) {
            return false;
        }
        if (b == 'P') break;
    }
    if (b != 'P') {
        return false;
    }

    cmd_frame_t f = {0};
    f.magic[0] = b;
    for (size_t i = 1; i < sizeof(f); ++i) {
        if (!t->read_with_timeout(((uint8_t *)&f) + i, 200)) {
            return false;
        }
    }

    if (f.magic[0] != 'P' || f.magic[1] != 'P' || f.magic[2] != 'K' || f.magic[3] != '1') {
        return false;
    }

    *out = f;
    return true;
}

static void fill_pattern(uint8_t seed) {
    for (size_t i = 0; i < TEST_BUF_SIZE; ++i) {
        s_test_buffer[i] = (uint8_t)(seed + (uint8_t)i);
    }
}

static uint16_t buffer_crc16(void) {
    return crc16_update(0, s_test_buffer, TEST_BUF_SIZE);
}

static bool do_exportbuf(void) {
    return packet_send_reliable(s_test_buffer, TEST_BUF_SIZE, 0);
}

static bool do_importbuf(void) {
    return packet_receive_reliable(s_test_buffer, TEST_BUF_SIZE, 0);
}

static bool do_stream32k(void) {
    const int pages = 64; // 64 * 512 = 32768 bytes
    for (int page = 0; page < pages; ++page) {
        for (size_t i = 0; i < TEST_BUF_SIZE; ++i) {
            s_test_buffer[i] = (uint8_t)((page + (int)i) & 0xFF);
        }
        if (!packet_send_reliable(s_test_buffer, TEST_BUF_SIZE, (uint8_t)page)) {
            return false;
        }
    }
    return true;
}

static bool do_import32k(uint16_t *out_crc) {
    uint16_t crc = 0;
    const int pages = 64;
    for (int page = 0; page < pages; ++page) {
        if (!packet_receive_reliable(s_test_buffer, TEST_BUF_SIZE, (uint8_t)page)) {
            return false;
        }
        crc = crc16_update(crc, s_test_buffer, TEST_BUF_SIZE);
    }
    *out_crc = crc;
    return true;
}

static bool do_get_version_stream(uint16_t *out_crc)
{
    uint8_t payload[FW_VERSION_PAYLOAD_SIZE] = {0};
    (void)snprintf(
        (char *)payload,
        sizeof(payload),
        "FW=%s BUILT=%s",
        FW_BUILD_TAG,
        FW_BUILD_UTC
    );

    uint16_t crc = crc16_update(0, payload, sizeof(payload));
    if (!packet_send_reliable(payload, sizeof(payload), 0)) {
        return false;
    }
    *out_crc = crc;
    return true;
}

static bool n64_claim_mode_for_cmd(uint8_t cmd)
{
    if (cmd < CMD_N64_RESCAN || cmd > CMD_N64_MX29LV640_EXPORT_WINDOW_STREAM) {
        return true;
    }

    joybus_mode_t mode = JOYBUS_MODE_CART_EEPROM;
    if (cmd == CMD_N64_CONTROLLER_PROBE ||
        cmd == CMD_N64_CONTROLLER_POLL ||
        cmd == CMD_N64_EXPORT_MPK ||
        cmd == CMD_N64_IMPORT_MPK) {
        mode = JOYBUS_MODE_CONTROLLER;
    }
    return joybus_set_mode(mode);
}

static bool n64_refresh_state(void) {
    s_n64_ready = gamepak_init();
    return s_n64_ready;
}

static void n64_drain_rx_quick(uint32_t quiet_us, size_t max_bytes) {
    const transport_t *t = transport_get();
    if (!t || !t->read_byte) {
        return;
    }

    uint8_t b = 0;
    size_t drained = 0;
    absolute_time_t deadline = make_timeout_time_us(quiet_us);

    while (drained < max_bytes && absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
        if (t->read_byte(&b)) {
            drained++;
            // Extend deadline while bytes are still arriving.
            deadline = make_timeout_time_us(quiet_us);
        } else {
            sleep_us(100);
        }
    }
}

static const n64_gamepak_info_t *n64_get_info_cached(void) {
    if (!s_n64_ready) {
        if (!n64_refresh_state()) {
            return NULL;
        }
    }
    if (!gamepak_is_present()) {
        return NULL;
    }
    return gamepak_get_info();
}

static bool n64_read_save_chunk(n64_save_type_t st, uint32_t offset, uint8_t *buf, size_t len) {
    switch (st) {
        case N64_SAVE_TYPE_SRAM:
            return gamepak_read_sram_bytes(N64_SRAM_BASE + offset, buf, len);
        case N64_SAVE_TYPE_EEPROM_4K:
        case N64_SAVE_TYPE_EEPROM_16K:
            return gamepak_read_eeprom_bytes(offset, buf, len);
        case N64_SAVE_TYPE_FLASHRAM:
            return gamepak_read_flashram_bytes(offset, buf, len);
        default:
            return false;
    }
}

static bool n64_write_save_chunk(n64_save_type_t st, uint32_t offset, const uint8_t *buf, size_t len) {
    switch (st) {
        case N64_SAVE_TYPE_SRAM:
            return gamepak_write_sram_bytes(N64_SRAM_BASE + offset, buf, len);
        case N64_SAVE_TYPE_EEPROM_4K:
        case N64_SAVE_TYPE_EEPROM_16K:
            return gamepak_write_and_verify_eeprom_bytes(offset, buf, len);
        default:
            return false;
    }
}

// Packs save metadata into 16 bits:
// [15:12] save_type, [11:0] size_units_64B (size_bytes / 64, rounded up).
static uint16_t n64_pack_save_meta(n64_save_type_t st, size_t size_bytes) {
    uint16_t units64 = (uint16_t)((size_bytes + 63u) / 64u);
    if (units64 > 0x0FFFu) {
        units64 = 0x0FFFu;
    }
    return (uint16_t)(((uint16_t)(st & 0x0Fu) << 12) | units64);
}

// Packs ROM size into 2KB units (size_bytes / 2048, rounded up).
// 64MB => 32768, fits in uint16_t.
static uint16_t n64_pack_rom_size_2k(size_t size_bytes) {
    uint32_t units2k = (uint32_t)((size_bytes + 2047u) / 2048u);
    if (units2k > 0xFFFFu) {
        units2k = 0xFFFFu;
    }
    return (uint16_t)units2k;
}

static bool n64_apply_host_save_profile(uint8_t save_type_raw, uint16_t size_units_64, uint16_t *out_meta) {
    const n64_gamepak_info_t *info = n64_get_info_cached();
    if (!info || !info->valid || !out_meta) {
        return false;
    }

    n64_save_type_t save_type = (n64_save_type_t)(save_type_raw & 0x0Fu);
    size_t size_bytes = (size_t)size_units_64 * 64u;
    if (!gamepak_set_save_profile(save_type, size_bytes)) {
        return false;
    }

    const n64_gamepak_info_t *updated = gamepak_get_info();
    if (!updated || !updated->valid) {
        return false;
    }

    *out_meta = n64_pack_save_meta(updated->save_type, updated->save_size_bytes);
    return true;
}

static bool n64_export_save_stream(uint16_t *out_crc) {
    const n64_gamepak_info_t *info = n64_get_info_cached();
    if (!info || !info->valid) {
        return false;
    }
    n64_save_type_t st = info->save_type;
    size_t save_size = info->save_size_bytes;
    if (save_size == 0 ||
        st == N64_SAVE_TYPE_NONE ||
        st == N64_SAVE_TYPE_UNKNOWN) {
        return false;
    }

    uint8_t chunk[512];
    uint16_t running_crc = 0;
    uint8_t seq = 0;

    for (uint32_t offset = 0; offset < save_size; offset += 512u) {
        size_t this_chunk = save_size - offset;
        if (this_chunk > sizeof(chunk)) {
            this_chunk = sizeof(chunk);
        }

        if (!n64_read_save_chunk(st, offset, chunk, this_chunk)) {
            return false;
        }
        running_crc = crc16_update(running_crc, chunk, this_chunk);
        if (!packet_send_reliable(chunk, (uint16_t)this_chunk, seq)) {
            return false;
        }
        seq++;

        // Tiny delay to let things settle before the next chunk.
        sleep_us(50);
    }

    *out_crc = running_crc;
    return true;
}

static bool n64_import_save_flashram(size_t save_size, uint16_t *out_crc) {
    if (save_size != N64_FLASHRAM_SIZE) {
        return false;
    }
    // Start from a known chip command state each import attempt.
    gamepak_flashram_reset_to_read_mode();
    if (!flashram_erase_block(N64_SRAM_BASE)) {
        gamepak_flashram_reset_to_read_mode();
        (void)n64_refresh_state();
        return false;
    }

    uint8_t chunk[512];
    uint16_t running_crc = 0;
    uint8_t seq = 0;
    const uint16_t page_size = 128u;

    for (uint32_t offset = 0; offset < save_size; offset += sizeof(chunk)) {
        if (!packet_receive_reliable(chunk, (uint16_t)sizeof(chunk), seq)) {
            gamepak_flashram_reset_to_read_mode();
            (void)n64_refresh_state();
            return false;
        }

        for (int p = 0; p < 4; ++p) {
            uint32_t page_addr = N64_SRAM_BASE + offset + ((uint32_t)p * page_size);
            if (!flashram_program_page(page_addr, &chunk[p * page_size])) {
                gamepak_flashram_reset_to_read_mode();
                (void)n64_refresh_state();
                return false;
            }
        }

        running_crc = crc16_update(running_crc, chunk, sizeof(chunk));
        seq++;
        sleep_ms(5);
    }

    *out_crc = running_crc;
    gamepak_flashram_reset_to_read_mode();
    return true;
}

static bool n64_import_save_stream(uint16_t *out_crc) {
    const n64_gamepak_info_t *info = n64_get_info_cached();
    if (!info || !info->valid) {
        return false;
    }
    n64_save_type_t st = info->save_type;
    size_t save_size = info->save_size_bytes;
    if (save_size == 0 ||
        st == N64_SAVE_TYPE_NONE ||
        st == N64_SAVE_TYPE_UNKNOWN) {
        return false;
    }

    if (st == N64_SAVE_TYPE_FLASHRAM) {
        return n64_import_save_flashram(save_size, out_crc);
    }

    uint8_t chunk[512];
    uint16_t running_crc = 0;
    uint8_t seq = 0;

    for (uint32_t offset = 0; offset < save_size; offset += 512u) {
        size_t this_chunk = save_size - offset;
        if (this_chunk > sizeof(chunk)) {
            this_chunk = sizeof(chunk);
        }

        if (!packet_receive_reliable(chunk, (uint16_t)this_chunk, seq)) {
            return false;
        }
        if (!n64_write_save_chunk(st, offset, chunk, this_chunk)) {
            return false;
        }
        running_crc = crc16_update(running_crc, chunk, this_chunk);
        seq++;
    }

    *out_crc = running_crc;
    return true;
}

static bool n64_export_sram_raw_stream(size_t raw_size, uint16_t *out_crc) {
    const n64_gamepak_info_t *info = n64_get_info_cached();
    if (!info || !info->valid) {
        if (out_crc) *out_crc = 0xE801u;
        return false;
    }
    if (!out_crc) {
        return false;
    }

    // Keep raw export bounded to common SRAM/FlashRAM save windows.
    if (raw_size == 0u || raw_size > N64_FLASHRAM_SIZE || (raw_size & 1u) != 0u) {
        *out_crc = 0xE802u;
        return false;
    }

    uint8_t chunk[512];
    uint16_t running_crc = 0;
    uint8_t seq = 0;

    for (uint32_t offset = 0; offset < raw_size; offset += sizeof(chunk)) {
        size_t this_chunk = raw_size - offset;
        if (this_chunk > sizeof(chunk)) {
            this_chunk = sizeof(chunk);
        }

        if (!gamepak_read_sram_bytes(N64_SRAM_BASE + offset, chunk, this_chunk)) {
            *out_crc = 0xE803u;
            return false;
        }
        running_crc = crc16_update(running_crc, chunk, this_chunk);
        if (!packet_send_reliable(chunk, (uint16_t)this_chunk, seq)) {
            *out_crc = 0xE804u;
            return false;
        }
        seq++;
    }

    *out_crc = running_crc;
    return true;
}

static bool n64_export_rom_stream(uint16_t *out_crc) {
    const n64_gamepak_info_t *info = n64_get_info_cached();
    if (!info || !info->valid || info->rom_size_bytes == 0) {
        return false;
    }
    size_t rom_size = info->rom_size_bytes;
    if (rom_size == 0) {
        return false;
    }

    uint8_t chunk[512];
    uint16_t running_crc = 0;
    uint8_t seq = 0;

    for (uint32_t offset = 0; offset < rom_size; offset += sizeof(chunk)) {
        size_t this_chunk = rom_size - offset;
        if (this_chunk > sizeof(chunk)) {
            this_chunk = sizeof(chunk);
        }

        // ROM reads are word-based; read an extra byte if a final odd byte exists.
        size_t read_len = this_chunk;
        if ((read_len & 1u) != 0u) {
            if (read_len >= sizeof(chunk)) {
                return false;
            }
            read_len++;
        }

        if (!gamepak_read_rom_bytes(N64_ROM_BASE + offset, chunk, read_len)) {
            return false;
        }
        running_crc = crc16_update(running_crc, chunk, this_chunk);
        if (!packet_send_reliable(chunk, (uint16_t)this_chunk, seq)) {
            return false;
        }
        seq++;
    }

    *out_crc = running_crc;
    return true;
}

static bool n64_export_header_stream(uint16_t *out_crc) {
    const n64_gamepak_info_t *info = n64_get_info_cached();
    if (!info || !info->valid) {
        return false;
    }

    const uint8_t *header = (const uint8_t *)&info->header;
    uint16_t running_crc = crc16_update(0, header, N64_HEADER_SIZE);
    if (!packet_send_reliable(header, N64_HEADER_SIZE, 0)) {
        return false;
    }

    *out_crc = running_crc;
    return true;
}

static bool n64_controller_probe_stream(uint16_t *out_crc) {
    n64_controller_info_t info = {0};
    if (!controller_probe(&info)) {
        return false;
    }

    uint8_t payload[3] = {
        (uint8_t)(info.device_id >> 8),
        (uint8_t)(info.device_id & 0xFF),
        info.status,
    };
    uint16_t running_crc = crc16_update(0, payload, sizeof(payload));
    if (!packet_send_reliable(payload, sizeof(payload), 0)) {
        return false;
    }

    *out_crc = running_crc;
    return true;
}

static bool n64_controller_poll_stream(bool rumble, uint16_t *out_crc) {
    n64_controller_state_t st = {0};
    if (!controller_read(&st, rumble)) {
        return false;
    }

    uint8_t payload[4] = {
        (uint8_t)(st.buttons >> 8),
        (uint8_t)(st.buttons & 0xFF),
        (uint8_t)st.stick_x,
        (uint8_t)st.stick_y,
    };
    uint16_t running_crc = crc16_update(0, payload, sizeof(payload));
    if (!packet_send_reliable(payload, sizeof(payload), 0)) {
        return false;
    }

    *out_crc = running_crc;
    return true;
}

static bool n64_export_mpk_stream(uint16_t *out_crc) {
    if (!controller_mempak_present()) {
        *out_crc = 0xE901u;
        return false;
    }

    uint8_t chunk[512];
    uint16_t running_crc = 0;
    uint8_t seq = 0;

    for (uint16_t offset = 0; offset < N64_MEMPAK_SIZE; offset += sizeof(chunk)) {
        for (uint16_t block = 0; block < sizeof(chunk); block += N64_MEMPAK_BLOCK_SIZE) {
            if (!controller_mempak_read_block((uint16_t)(offset + block), &chunk[block])) {
                *out_crc = (uint16_t)(0xEA00u | (((offset + block) / N64_MEMPAK_BLOCK_SIZE) & 0x00FFu));
                return false;
            }
            sleep_us(1200);
        }
        running_crc = crc16_update(running_crc, chunk, sizeof(chunk));
        if (!packet_send_reliable(chunk, sizeof(chunk), seq)) {
            return false;
        }
        seq++;
    }

    *out_crc = running_crc;
    return true;
}

static bool n64_import_mpk_stream(uint16_t *out_crc) {
    if (!controller_mempak_present()) {
        *out_crc = 0xEB01u;
        return false;
    }

    uint8_t chunk[512];
    uint16_t running_crc = 0;
    uint8_t seq = 0;

    for (uint16_t offset = 0; offset < N64_MEMPAK_SIZE; offset += sizeof(chunk)) {
        if (!packet_receive_reliable(chunk, sizeof(chunk), seq)) {
            *out_crc = (uint16_t)(0xEC00u | (seq & 0x00FFu));
            return false;
        }
        for (uint16_t block = 0; block < sizeof(chunk); block += N64_MEMPAK_BLOCK_SIZE) {
            if (!controller_mempak_write_block((uint16_t)(offset + block), &chunk[block])) {
                *out_crc = (uint16_t)(0xED00u | (((offset + block) / N64_MEMPAK_BLOCK_SIZE) & 0x00FFu));
                return false;
            }
            sleep_us(1200);
        }
        running_crc = crc16_update(running_crc, chunk, sizeof(chunk));
        seq++;
    }

    *out_crc = running_crc;
    return true;
}

static bool n64_gs_export_stream(uint16_t *out_crc) {
    if (!out_crc) {
        return false;
    }
    *out_crc = 0xE700u;

    n64_gs_info_t info = {0};
    if (!gamepak_gs_probe(&info) || !info.present) {
        *out_crc = 0xE701u;
        return false;
    }
    if (info.size_bytes == 0u || (info.size_bytes & 1u) != 0u) {
        *out_crc = 0xE702u;
        return false;
    }

    uint8_t chunk[512];
    uint16_t running_crc = 0;
    uint8_t seq = 0;
    uint32_t dump_size = info.size_bytes;

    gamepak_gs_unlock();

    for (uint32_t offset = 0; offset < dump_size; offset += sizeof(chunk)) {
        size_t this_chunk = dump_size - offset;
        if (this_chunk > sizeof(chunk)) {
            this_chunk = sizeof(chunk);
        }
        if ((this_chunk & 1u) != 0u) {
            *out_crc = 0xE706u;
            return false;
        }
        if (!gamepak_gs_read_bytes(offset, chunk, this_chunk)) {
            *out_crc = 0xE703u;
            return false;
        }
        running_crc = crc16_update(running_crc, chunk, this_chunk);
        if (!packet_send_reliable(chunk, (uint16_t)this_chunk, seq)) {
            *out_crc = 0xE704u;
            return false;
        }
        seq++;
    }

    *out_crc = running_crc;
    return true;
}

static bool n64_gs_import_stream(uint16_t *out_crc) {
    if (!out_crc) {
        return false;
    }
    *out_crc = 0xE800u;

    n64_gs_info_t info = {0};
    if (!gamepak_gs_probe(&info) || !info.present) {
        *out_crc = 0xE801u;
        return false;
    }
    if ((info.caps & N64_GS_CAP_IMPORT) == 0 || (info.caps & N64_GS_CAP_ERASE) == 0) {
        *out_crc = 0xE802u;
        return false;
    }
    if (info.size_bytes == 0u || (info.size_bytes & 1u) != 0u) {
        *out_crc = 0xE803u;
        return false;
    }

    gamepak_gs_unlock();
    if (!gamepak_gs_erase(info.flash_id)) {
        *out_crc = 0xE804u;
        return false;
    }

    uint8_t chunk[512];
    uint16_t running_crc = 0;
    uint8_t seq = 0;
    uint32_t dump_size = info.size_bytes;

    for (uint32_t offset = 0; offset < dump_size; offset += sizeof(chunk)) {
        size_t this_chunk = dump_size - offset;
        if (this_chunk > sizeof(chunk)) {
            this_chunk = sizeof(chunk);
        }

        if (!packet_receive_reliable(chunk, (uint16_t)this_chunk, seq)) {
            *out_crc = (uint16_t)(0xE900u | (seq & 0x00FFu));
            return false;
        }

        if ((this_chunk & 1u) != 0u) {
            *out_crc = 0xE806u;
            return false;
        }

        if (!gamepak_gs_write_bytes(info.flash_id, offset, chunk, this_chunk)) {
            *out_crc = 0xE805u;
            return false;
        }
        running_crc = crc16_update(running_crc, chunk, this_chunk);
        seq++;
    }

    *out_crc = running_crc;
    return true;
}

void headless_protocol_init(void) {
    memset(s_test_buffer, 0, sizeof(s_test_buffer));
    s_n64_ready = gamepak_init();
}

void headless_protocol_poll(void) {
    cmd_frame_t cmd = {0};
    if (!read_command_frame(&cmd)) {
        return;
    }

    const uint16_t arg1 = be16_decode(cmd.arg1_be);

    if (!n64_claim_mode_for_cmd(cmd.cmd)) {
        (void)send_response(cmd.cmd, ST_IO_ERR, 0);
        return;
    }

    switch (cmd.cmd) {
        case CMD_PING:
            (void)send_response(cmd.cmd, ST_OK, 0xCAFE);
            break;

        case CMD_GET_INFO:
            // value: test-buffer size in bytes
            (void)send_response(cmd.cmd, ST_OK, (uint16_t)TEST_BUF_SIZE);
            break;

        case CMD_GET_VERSION: {
            uint16_t running_crc = 0;
            if (do_get_version_stream(&running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;
        }

        case CMD_FILLBUF:
            fill_pattern(cmd.arg0);
            (void)send_response(cmd.cmd, ST_OK, (uint16_t)TEST_BUF_SIZE);
            break;

        case CMD_CLEARBUF:
            memset(s_test_buffer, 0, sizeof(s_test_buffer));
            (void)send_response(cmd.cmd, ST_OK, 0);
            break;

        case CMD_WRITEMSG: {
            const char *msg = "Mika, Taz, Dash, Patch, Daisy (Doo), Shadow";
            size_t len = strlen(msg);
            if (len >= TEST_BUF_SIZE) {
                len = TEST_BUF_SIZE - 1;
            }
            memcpy(s_test_buffer, msg, len);
            s_test_buffer[len] = '\0';
            (void)send_response(cmd.cmd, ST_OK, (uint16_t)len);
            break;
        }

        case CMD_SWAP16: {
            size_t nwords = arg1 ? (size_t)arg1 : (TEST_BUF_SIZE / 2u);
            if (nwords > (TEST_BUF_SIZE / 2u)) {
                nwords = TEST_BUF_SIZE / 2u;
            }
            byteswap16_buf((uint16_t *)s_test_buffer, nwords);
            (void)send_response(cmd.cmd, ST_OK, (uint16_t)nwords);
            break;
        }

        case CMD_SWAP32: {
            size_t nwords = arg1 ? (size_t)arg1 : (TEST_BUF_SIZE / 4u);
            if (nwords > (TEST_BUF_SIZE / 4u)) {
                nwords = TEST_BUF_SIZE / 4u;
            }
            byteswap32_buf((uint32_t *)s_test_buffer, nwords);
            (void)send_response(cmd.cmd, ST_OK, (uint16_t)nwords);
            break;
        }

        case CMD_CRC16:
            (void)send_response(cmd.cmd, ST_OK, buffer_crc16());
            break;

        case CMD_EXPORTBUF:
            if (do_exportbuf()) {
                (void)send_response(cmd.cmd, ST_OK, (uint16_t)TEST_BUF_SIZE);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;

        case CMD_IMPORTBUF:
            if (do_importbuf()) {
                (void)send_response(cmd.cmd, ST_OK, buffer_crc16());
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;

        case CMD_STREAM32K:
            if (do_stream32k()) {
                (void)send_response(cmd.cmd, ST_OK, 32768u);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;

        case CMD_IMPORT32K: {
            uint16_t running_crc = 0;
            if (do_import32k(&running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;
        }

        case CMD_N64_RESCAN:
            if (n64_refresh_state()) {
                (void)send_response(cmd.cmd, ST_OK, 1);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;

        case CMD_N64_STATUS: {
            const n64_gamepak_info_t *info = n64_get_info_cached();
            if (!info || !info->valid) {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
                break;
            }
            uint16_t meta = n64_pack_save_meta(info->save_type, info->save_size_bytes);
            (void)send_response(cmd.cmd, ST_OK, meta);
            break;
        }

        case CMD_N64_EXPORT_SAVE: {
            uint16_t running_crc = 0;
            if (n64_export_save_stream(&running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;
        }

        case CMD_N64_IMPORT_SAVE: {
            uint16_t running_crc = 0;
            if (n64_import_save_stream(&running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                // After stream failures, discard trailing bytes from in-flight
                // host retries so next command starts on a clean frame boundary.
                // 1s quiet window covers host's 500ms retry interval; 128KB cap
                // covers a full FlashRAM import's worth of stale data.
                n64_drain_rx_quick(1000000u, 131072u);
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;
        }

        case CMD_N64_ROM_INFO: {
            const n64_gamepak_info_t *info = n64_get_info_cached();
            if (!info || !info->valid || info->rom_size_bytes == 0) {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
                break;
            }
            uint16_t size_2k = n64_pack_rom_size_2k(info->rom_size_bytes);
            (void)send_response(cmd.cmd, ST_OK, size_2k);
            break;
        }

        case CMD_N64_EXPORT_ROM: {
            uint16_t running_crc = 0;
            if (n64_export_rom_stream(&running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;
        }

        case CMD_N64_EXPORT_HEADER: {
            uint16_t running_crc = 0;
            if (n64_export_header_stream(&running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;
        }

        case CMD_N64_CONTROLLER_PROBE: {
            uint16_t running_crc = 0;
            if (n64_controller_probe_stream(&running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;
        }

        case CMD_N64_CONTROLLER_POLL: {
            uint16_t running_crc = 0;
            bool rumble = (cmd.arg0 != 0);
            if (n64_controller_poll_stream(rumble, &running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            }
            break;
        }

        case CMD_N64_EXPORT_MPK: {
            uint16_t running_crc = 0;
            if (n64_export_mpk_stream(&running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, running_crc);
            }
            break;
        }

        case CMD_N64_IMPORT_MPK: {
            uint16_t running_crc = 0;
            if (n64_import_mpk_stream(&running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, running_crc);
            }
            break;
        }

        case CMD_N64_GS_INFO: {
            n64_gs_info_t info = {0};
            bool present = gamepak_gs_probe(&info);
            if (!packet_send_reliable((const uint8_t *)&info, sizeof(info), 0)) {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            } else {
                (void)send_response(cmd.cmd, present ? ST_OK : ST_IO_ERR, info.flash_id);
            }
            break;
        }

        case CMD_N64_GS_EXPORT: {
            uint16_t running_crc = 0;
            if (n64_gs_export_stream(&running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, running_crc);
            }
            break;
        }

        case CMD_N64_GS_IMPORT: {
            uint16_t running_crc = 0;
            if (n64_gs_import_stream(&running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                n64_drain_rx_quick(1000000u, 1048576u); // Drain up to 1MB of stale data
                (void)send_response(cmd.cmd, ST_IO_ERR, running_crc);
            }
            break;
        }

        case CMD_N64_EXPORT_SRAM_RAW: {
            uint16_t running_crc = 0;
            size_t raw_size = (size_t)(arg1 ? arg1 : 64u) * 512u;
            if (n64_export_sram_raw_stream(raw_size, &running_crc)) {
                (void)send_response(cmd.cmd, ST_OK, running_crc);
            } else {
                (void)send_response(cmd.cmd, ST_IO_ERR, running_crc);
            }
            break;
        }

        case CMD_N64_SET_SAVE_CFG: {
             const n64_gamepak_info_t *info = n64_get_info_cached();
             if (!info || !info->valid) {
                 (void)send_response(cmd.cmd, ST_IO_ERR, 0);
                 break;
             }
             uint16_t meta = 0;
             if (n64_apply_host_save_profile(cmd.arg0, arg1, &meta)) {
                 (void)send_response(cmd.cmd, ST_OK, meta);
             } else {
                 (void)send_response(cmd.cmd, ST_BAD_ARG, 0);
             }
             break;
         }

        case CMD_N64_SET_ROM_SIZE: {
            uint32_t size_bytes = (uint32_t)arg1 * 2048u;
            if (gamepak_set_rom_size(size_bytes)) {
                (void)send_response(cmd.cmd, ST_OK, n64_pack_rom_size_2k(size_bytes));
            } else {
                (void)send_response(cmd.cmd, ST_BAD_ARG, 0);
            }
            break;
        }

        // Experimental repro-cart ROM/NOR lab commands.
        // These are intentionally separate from the normal N64 ROM dump/read
        // path and should not be treated as generic cart operations.
        case CMD_N64_ROM_FLASH_ID: {
            n64_rom_flash_id_t flash_id = {0};
            bool present = gamepak_rom_flash_probe(&flash_id);
            if (!packet_send_reliable((const uint8_t *)&flash_id, sizeof(flash_id), 0)) {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            } else {
                (void)send_response(cmd.cmd, present ? ST_OK : ST_IO_ERR, flash_id.mfg_id);
            }
            break;
        }

        case CMD_N64_ROM_FLASH_ERASE: {
            n64_rom_flash_erase_result_t result = {0};
            bool ok = gamepak_rom_flash_chip_erase(&result);
            if (!packet_send_reliable((const uint8_t *)&result, sizeof(result), 0)) {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            } else {
                (void)send_response(cmd.cmd, ok ? ST_OK : ST_IO_ERR, result.poll_count);
            }
            break;
        }

        case CMD_N64_ROM_FLASH_WRITE_TEST: {
            n64_repro_alias_diag_t result = {0};
            gamepak_repro_alias_diag_read(&result);
            if (!packet_send_reliable((const uint8_t *)&result, sizeof(result), 0)) {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            } else {
                (void)send_response(cmd.cmd, ST_OK, 0);
            }
            break;
        }

        case CMD_N64_REPRO_READ_WORD: {
            // Read a single ROM word at an arbitrary offset. This is meant for
            // low-level repro diagnosis, not general ROM dumping. Host tools
            // can optionally send a reset carrier first if the cart is not
            // already in a good raw read state.
            // arg0 = bits [24:17] of byte offset (upper 8 bits)
            // arg1 = bits [16:1]  of byte offset (lower 16 bits, must be even)
            // Reconstructed offset = (arg0 << 17) | (arg1 & 0xFFFE)
            // Max addressable: (0xFF << 17) | 0xFFFE = 0x01FFFFFE = ~32 MB
            uint32_t offset = ((uint32_t)cmd.arg0 << 17) | (arg1 & 0xFFFEu);
            uint8_t raw_word[2] = {0};
            if (!gamepak_repro_read_rom_bytes(N64_ROM_BASE + offset, raw_word, sizeof(raw_word))) {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            } else {
                uint16_t word = ((uint16_t)raw_word[0] << 8) | raw_word[1];
                (void)send_response(cmd.cmd, ST_OK, word);
            }
            break;
        }

        case CMD_N64_MX29LV640_EXPORT_WINDOW_STREAM: {
            enum { WINDOW_LEN = 512 };
            const uint32_t word_offset = ((uint32_t)cmd.arg0 << 16) | arg1;
            const uint32_t byte_offset = word_offset << 1;
            if (byte_offset > 0x007FFE00u) {
                (void)send_response(cmd.cmd, ST_BAD_ARG, (uint16_t)(byte_offset >> 1));
                break;
            }

            if (!gamepak_mx29lv640_stream_read_window(
                    N64_ROM_BASE + byte_offset,
                    s_test_buffer,
                    WINDOW_LEN)) {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
                break;
            }

            if (!packet_send_reliable(s_test_buffer, WINDOW_LEN, 0)) {
                (void)send_response(cmd.cmd, ST_IO_ERR, 0);
            } else {
                (void)send_response(cmd.cmd, ST_OK, (uint16_t)(byte_offset >> 1));
            }
            break;
        }

         default:            (void)send_response(cmd.cmd, ST_BAD_CMD, 0);
            break;
    }
}
