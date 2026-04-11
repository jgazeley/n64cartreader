/**
 * @file     gamepak_repro.c
 * @brief    ROM-space NOR helpers for N64 repro cartridges.
 */

#include "n64/devices/gamepak_repro.h"

#include "n64/bus/adbus.h"
#include "n64/devices/gamepak.h"

#include "hardware/structs/sio.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include <string.h>

// AMD/Fujitsu unlock sequence addresses (byte addresses in N64 ROM space).
// In x16 mode: word addr 0x555 = byte addr 0xAAA, word addr 0x2AA = byte addr 0x554.
#define ROM_FLASH_UNLOCK1_ADDR  (N64_ROM_BASE + 0x0AAAu)
#define ROM_FLASH_UNLOCK2_ADDR  (N64_ROM_BASE + 0x0554u)
#define ROM_FLASH_CMD_ADDR      (N64_ROM_BASE + 0x0AAAu)
#define ROM_FLASH_RESET_ADDR    (N64_ROM_BASE)

// Raw repro bus helpers. These intentionally bypass adbus_latch_address() and
// emulate the proven direct SIO sequence from earlier lab work.
static void _repro_raw_write_word(uint32_t addr, uint16_t data) {
    uint16_t hi = addr >> 16;
    uint16_t lo = (uint16_t)addr;

    // -- setAddress_N64 equivalent --
    // WR=HIGH, RD=HIGH
    sio_hw->gpio_set = (1u << N64_ADBUS_WR_PIN) | (1u << N64_ADBUS_RD_PIN);
    // ALE_H HIGH first
    sio_hw->gpio_set = (1u << N64_ADBUS_ALE_H_PIN);
    __asm volatile("nop\n");  // "needed for repro" per sanni
    // ALE_L HIGH
    sio_hw->gpio_set = (1u << N64_ADBUS_ALE_L_PIN);

    // High word on bus
    sio_hw->gpio_clr = N64_ADBUS_GPIO_MASK;
    sio_hw->gpio_set = (uint32_t)hi << N64_ADBUS_PIN_START;
    __asm volatile("nop\n");
    // ALE_H LOW -> latch high word
    sio_hw->gpio_clr = (1u << N64_ADBUS_ALE_H_PIN);

    // Low word on bus
    sio_hw->gpio_clr = N64_ADBUS_GPIO_MASK;
    sio_hw->gpio_set = (uint32_t)lo << N64_ADBUS_PIN_START;
    __asm volatile("nop\nnop\n");
    // ALE_L LOW -> latch low word
    sio_hw->gpio_clr = (1u << N64_ADBUS_ALE_L_PIN);

    // Data on bus
    sio_hw->gpio_clr = N64_ADBUS_GPIO_MASK;
    sio_hw->gpio_set = (uint32_t)data << N64_ADBUS_PIN_START;
    __asm volatile("nop\n");  // data setup
    // WR LOW
    sio_hw->gpio_clr = (1u << N64_ADBUS_WR_PIN);
    // WR pulse width (~400ns)
    for (volatile int i = 0; i < 50; i++) {
        __asm volatile("nop\n");
    }
    // WR HIGH
    sio_hw->gpio_set = (1u << N64_ADBUS_WR_PIN);
    __asm volatile("nop\nnop\n");
}

static void _repro_raw_latch_address(uint32_t addr) {
    uint16_t hi = addr >> 16;
    uint16_t lo = (uint16_t)addr;

    sio_hw->gpio_set = (1u << N64_ADBUS_WR_PIN) | (1u << N64_ADBUS_RD_PIN);
    sio_hw->gpio_set = (1u << N64_ADBUS_ALE_H_PIN);
    __asm volatile("nop\n");
    sio_hw->gpio_set = (1u << N64_ADBUS_ALE_L_PIN);

    adbus_set_direction(true);

    sio_hw->gpio_clr = N64_ADBUS_GPIO_MASK;
    sio_hw->gpio_set = (uint32_t)hi << N64_ADBUS_PIN_START;
    __asm volatile("nop\n");
    sio_hw->gpio_clr = (1u << N64_ADBUS_ALE_H_PIN);

    sio_hw->gpio_clr = N64_ADBUS_GPIO_MASK;
    sio_hw->gpio_set = (uint32_t)lo << N64_ADBUS_PIN_START;
    __asm volatile("nop\nnop\n");
    sio_hw->gpio_clr = (1u << N64_ADBUS_ALE_L_PIN);

    adbus_set_direction(false);
}

static uint16_t _repro_raw_read_word(void) {
    sio_hw->gpio_clr = (1u << N64_ADBUS_RD_PIN);
    __asm volatile("nop\nnop\nnop\nnop\nnop\n");
    uint32_t port_val = sio_hw->gpio_in;
    sio_hw->gpio_set = (1u << N64_ADBUS_RD_PIN);
    return (uint16_t)((port_val & N64_ADBUS_GPIO_MASK) >> N64_ADBUS_PIN_START);
}

void gamepak_repro_flash_reset(void) {
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
}

void gamepak_repro_prepare_read_array(void) {
    adbus_set_direction(true);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(200);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(200);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(500);
    adbus_set_direction(false);
}

static void _mx29lv640_prepare_read_array(void) {
    // Match the older working MX29 path: one read-array reset, then a long
    // settle before the one-latch sequential read stream.
    adbus_set_direction(true);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_ms(100);
    adbus_set_direction(false);
}

bool gamepak_repro_read_rom_bytes(uint32_t rom_address, uint8_t *buffer, size_t length) {
    if ((!buffer && length != 0) || (length & 1u) != 0u) {
        return false;
    }
    if (length == 0) {
        return true;
    }

    gamepak_repro_prepare_read_array();
    _repro_raw_latch_address(rom_address);

    // The proven raw-header probes showed a stale first word followed by an
    // aligned N64 header. Discard one read after each latch for this backend.
    (void)_repro_raw_read_word();

    for (size_t i = 0; i < length; i += 2u) {
        uint16_t word = _repro_raw_read_word();
        buffer[i] = (uint8_t)(word >> 8);
        buffer[i + 1] = (uint8_t)(word & 0xFF);
    }

    gamepak_bus_warmup();
    return true;
}

bool gamepak_mx29lv640_stream_read_window(uint32_t rom_address, uint8_t *buffer, uint16_t length) {
    if (!buffer || length == 0u || (length & 1u) != 0u) {
        return false;
    }

    _mx29lv640_prepare_read_array();
    _repro_raw_latch_address(rom_address);

    for (uint16_t i = 0; i < length; i += 2u) {
        uint16_t word = _repro_raw_read_word();
        buffer[i] = (uint8_t)(word >> 8);
        buffer[i + 1] = (uint8_t)(word & 0xFF);
    }

    gamepak_bus_warmup();
    return true;
}

bool gamepak_mx29lv640_read_rom_bytes(uint32_t rom_address, uint8_t *buffer, size_t length) {
    if ((!buffer && length != 0u) || (length & 1u) != 0u) {
        return false;
    }
    if (length == 0u) {
        return true;
    }

    // The bulk-read path for this board only needs a brief read-array nudge.
    adbus_set_direction(true);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(50);
    adbus_set_direction(false);

    _repro_raw_latch_address(rom_address);
    for (size_t i = 0; i < length; i += 2u) {
        uint16_t word = _repro_raw_read_word();
        buffer[i] = (uint8_t)(word >> 8);
        buffer[i + 1] = (uint8_t)(word & 0xFF);
    }

    gamepak_bus_warmup();
    return true;
}

bool gamepak_rom_flash_chip_erase(n64_rom_flash_erase_result_t *diag) {
    // AMD/Fujitsu chip erase command sequence (x16 mode):
    //   1. Unlock: 0xAA -> word 0x555, 0x55 -> word 0x2AA
    //   2. Erase setup: 0x80 -> word 0x555
    //   3. Unlock: 0xAA -> word 0x555, 0x55 -> word 0x2AA
    //   4. Chip erase: 0x10 -> word 0x555
    // Then poll DQ7 until erase completes.

    if (diag) {
        memset(diag, 0, sizeof(*diag));
    }

    // Read pre-erase word at offset 0 for diagnostics
    adbus_latch_address(N64_ROM_BASE);
    uint16_t pre = adbus_read_word();
    if (diag) {
        diag->pre_word = pre;
    }

    adbus_set_direction(true);

    // Software reset first
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(100);

    // Erase setup: unlock + 0x80
    _repro_raw_write_word(ROM_FLASH_UNLOCK1_ADDR, 0x00AA);
    _repro_raw_write_word(ROM_FLASH_UNLOCK2_ADDR, 0x0055);
    _repro_raw_write_word(ROM_FLASH_CMD_ADDR,     0x0080);

    // Chip erase: unlock + 0x10
    _repro_raw_write_word(ROM_FLASH_UNLOCK1_ADDR, 0x00AA);
    _repro_raw_write_word(ROM_FLASH_UNLOCK2_ADDR, 0x0055);
    _repro_raw_write_word(ROM_FLASH_CMD_ADDR,     0x0010);

    // Switch to input for status polling
    adbus_set_direction(false);

    // Poll DQ7: during erase DQ7=0, when done DQ7=1.
    // Check: (statusReg | 0xFF7F) == 0xFFFF means bit 7 is set -> done.
    // S29GL256N typical chip erase: 120s, max 480s.
    bool done = false;
    uint16_t first_sr = 0;
    uint16_t last_sr = 0;
    int count = 0;
    for (int i = 0; i < 600; i++) {
        tud_task();  // keep USB alive

        adbus_latch_address(N64_ROM_BASE);
        uint16_t sr = adbus_read_word();
        if (i == 0) {
            first_sr = sr;
        }
        last_sr = sr;
        count = i + 1;

        if ((sr | 0xFF7Fu) == 0xFFFFu) {
            done = true;
            break;
        }

        sleep_ms(1000);
    }

    // Reset to read-array mode
    adbus_set_direction(true);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(100);
    adbus_set_direction(false);

    // Read post-erase word at offset 0
    adbus_latch_address(N64_ROM_BASE);
    uint16_t post = adbus_read_word();

    if (diag) {
        diag->first_sr = first_sr;
        diag->final_sr = last_sr;
        diag->post_word = post;
        diag->poll_count = (uint16_t)count;
        diag->ok = done ? 1 : 0;
    }

    return done;
}

bool gamepak_rom_flash_probe_diag(n64_rom_flash_diag_t *diag) {
    if (!diag) {
        return false;
    }
    memset(diag, 0, sizeof(*diag));

    // Read 32 words in normal mode (byte offsets 0x00-0x3E)
    for (int i = 0; i < 32; i++) {
        adbus_latch_address(N64_ROM_BASE + (uint32_t)(i * 2));
        diag->normal[i] = adbus_read_word();
    }

    // Enter autoselect mode (not CFI) and read 32 words
    adbus_set_direction(true);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(100);
    _repro_raw_write_word(ROM_FLASH_UNLOCK1_ADDR, 0x00AA);
    _repro_raw_write_word(ROM_FLASH_UNLOCK2_ADDR, 0x0055);
    _repro_raw_write_word(ROM_FLASH_CMD_ADDR,     0x0090);
    sleep_us(50);
    adbus_set_direction(false);

    // Read 32 words from byte offsets 0x00-0x3E in autoselect mode
    for (int i = 0; i < 32; i++) {
        adbus_latch_address(N64_ROM_BASE + (uint32_t)(i * 2));
        diag->query[i] = adbus_read_word();
    }

    // Reset back to read-array mode
    adbus_set_direction(true);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(100);
    adbus_set_direction(false);

    return true;
}

bool gamepak_rom_flash_probe(n64_rom_flash_id_t *out) {
    if (!out) {
        return false;
    }

    // Force bus to output - syncs internal state flag.
    adbus_set_direction(true);

    // Software reset
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(100);

    // AMD/Fujitsu unlock + autoselect
    _repro_raw_write_word(ROM_FLASH_UNLOCK1_ADDR, 0x00AA);
    _repro_raw_write_word(ROM_FLASH_UNLOCK2_ADDR, 0x0055);
    _repro_raw_write_word(ROM_FLASH_CMD_ADDR,     0x0090);
    sleep_us(50);

    // Switch to input for reads
    adbus_set_direction(false);

    adbus_latch_address(N64_ROM_BASE + 0x0000u);
    out->mfg_id = adbus_read_word();
    adbus_latch_address(N64_ROM_BASE + 0x0002u);
    out->dev_id0 = adbus_read_word();
    adbus_latch_address(N64_ROM_BASE + 0x001Cu);
    out->dev_id1 = adbus_read_word();
    adbus_latch_address(N64_ROM_BASE + 0x001Eu);
    out->dev_id2 = adbus_read_word();

    // Reset back to read-array mode
    adbus_set_direction(true);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(100);
    adbus_set_direction(false);

    bool looks_open_bus = (out->mfg_id == 0x0000u || out->mfg_id == 0xFFFFu);
    if (looks_open_bus) {
        return false;
    }

    // Retail mask ROMs ignore the autoselect sequence and simply keep serving
    // the normal ROM header. The common false-positive signature is
    // 0x8037 / 0x1240, which are just the first two header words.
    uint16_t header_word0 = 0;
    uint16_t header_word1 = 0;
    adbus_latch_address(N64_ROM_BASE + 0x0000u);
    header_word0 = adbus_read_word();
    adbus_latch_address(N64_ROM_BASE + 0x0002u);
    header_word1 = adbus_read_word();
    bool looks_like_header_echo = (out->mfg_id == header_word0 && out->dev_id0 == header_word1);
    if (looks_like_header_echo) {
        memset(out, 0, sizeof(*out));
        return false;
    }

    return true;
}

bool gamepak_rom_flash_write_buffer(uint32_t addr, const uint8_t *data, uint8_t len) {
    if (!data || len == 0 || len > 32 || (len & 1u)) {
        return false;
    }

    // Claude's proven recipe for this counterfeit NOR:
    // - _sanni_write for every command and data word
    // - 50us between unlock/command/data writes
    // - +2 byte compensation on the DATA write address only
    // - fixed delay + readback verification instead of DQ polling
    for (uint8_t i = 0; i < len; i += 2) {
        uint16_t word = ((uint16_t)data[i] << 8) | data[i + 1];

        adbus_set_direction(true);

        _repro_raw_write_word(ROM_FLASH_UNLOCK1_ADDR, 0x00AA);
        sleep_us(50);
        _repro_raw_write_word(ROM_FLASH_UNLOCK2_ADDR, 0x0055);
        sleep_us(50);
        _repro_raw_write_word(ROM_FLASH_CMD_ADDR,     0x00A0);
        sleep_us(50);
        _repro_raw_write_word(addr + i + 2u, word);
        sleep_us(50);

        // Fixed delay for programming (DQ5 status unreliable on counterfeit)
        sleep_us(1000);

        // Reset to read-array mode
        _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
        sleep_us(200);
    }

    // Verify: triple-reset then read back
    adbus_set_direction(true);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(200);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(200);
    _repro_raw_write_word(ROM_FLASH_RESET_ADDR, 0x00F0);
    sleep_us(500);
    adbus_set_direction(false);

    for (uint8_t i = 0; i < len; i += 2) {
        uint16_t expected = ((uint16_t)data[i] << 8) | data[i + 1];
        adbus_latch_address(addr + i);
        uint16_t actual = adbus_read_word();
        if (actual != expected) {
            return false;
        }
    }

    return true;
}

void gamepak_repro_alias_diag_read(n64_repro_alias_diag_t *diag) {
    if (!diag) {
        return;
    }
    memset(diag, 0, sizeof(*diag));

    // Non-destructive aliasing diagnostic: force the repro back into
    // read-array mode, then read a fixed set of power-of-two offsets. If
    // those distant reads return sequential header words instead of 0xFFFF,
    // the CPLD/address decode path is aliasing or scrambling address bits.

    gamepak_repro_prepare_read_array();

    static const uint32_t offsets[N64_REPRO_ALIAS_DIAG_WORD_COUNT] = {
        0x00000, 0x00002, 0x00800, 0x01000,
        0x02000, 0x04000, 0x08000, 0x10000,
        0x20000, 0x40000, 0x80000, 0x100000,
        0x200000
    };

    for (int i = 0; i < N64_REPRO_ALIAS_DIAG_WORD_COUNT; ++i) {
        adbus_latch_address(N64_ROM_BASE + offsets[i]);
        diag->words[i] = adbus_read_word();
    }
}
