#include "n64/devices/gameshark.h"
#include "n64/bus/adbus.h"
#include "pico/stdlib.h"
#include <string.h>

#define N64_GS_UNLOCK_ADDR      0x10400400u
#define N64_GS_ROM_BASE         0x1EC00000u
#define N64_GS_CMD_ADDR_1       0x1EF0AAA8u
#define N64_GS_CMD_ADDR_2       0x1EE05554u
#define N64_GS_ID_ADDR          0x1EC00000u

#define N64_GS_DEV_ID_SST29LE010  0x0808u
#define N64_GS_DEV_ID_SST28LF040  0x0404u
#define N64_GS_DEV_ID_AT29LV010A  0x3535u
#define N64_GS_DEV_ID_SST29EE010  0x0707u
#define N64_GS_DEV_ID_UNKNOWN_1208 0x1208u

#define N64_GS_SIZE_256K          262144u
#define N64_GS_SIZE_512K          524288u
#define N64_GS_SIZE_1M            1048576u

static bool gs_is_known_device_id(uint16_t dev_id)
{
    switch (dev_id) {
        case N64_GS_DEV_ID_SST29LE010:
        case N64_GS_DEV_ID_SST28LF040:
        case N64_GS_DEV_ID_AT29LV010A:
        case N64_GS_DEV_ID_SST29EE010:
        case N64_GS_DEV_ID_UNKNOWN_1208:
            return true;
        default:
            return false;
    }
}

static uint32_t gs_size_for_device_id(uint16_t dev_id)
{
    if (dev_id == N64_GS_DEV_ID_SST28LF040) {
        return N64_GS_SIZE_1M;
    }
    if (dev_id == N64_GS_DEV_ID_UNKNOWN_1208 || gs_is_known_device_id(dev_id)) {
        return N64_GS_SIZE_256K;
    }
    return 0;
}

static bool gs_word_looks_open_bus(uint16_t word)
{
    return (word == 0xFFFFu || word == 0x0000u);
}

static void gamepak_gs_send_command(uint16_t cmd)
{
    adbus_set_direction(true);
    adbus_latch_address(N64_GS_CMD_ADDR_1);
    adbus_write_word(0xAAAA);
    adbus_latch_address(N64_GS_CMD_ADDR_2);
    adbus_write_word(0x5555);
    adbus_latch_address(N64_GS_CMD_ADDR_1);
    adbus_write_word(cmd);
    adbus_set_direction(false);
}

void gamepak_gs_reset_to_read_mode(uint16_t flash_id)
{
    gamepak_gs_send_command(flash_id == N64_GS_DEV_ID_SST28LF040 ? 0xFFFF : 0xF0F0);
    sleep_us(flash_id == N64_GS_DEV_ID_SST28LF040 ? 300 : 100);
}

void gamepak_gs_unlock(void) {
    adbus_set_direction(true);
    adbus_latch_address(N64_GS_UNLOCK_ADDR);
    adbus_write_word(0x001E);
    adbus_write_word(0x001E);
    adbus_set_direction(false);
}

bool gamepak_gs_probe(n64_gs_info_t *out_info) {
    if (!out_info) {
        return false;
    }
    memset(out_info, 0, sizeof(*out_info));

    uint16_t final_mfg = 0xFFFFu;
    uint16_t final_dev = 0xFFFFu;
    uint16_t flags = 0;

    for (int attempt = 0; attempt < 3; ++attempt) {
        gamepak_gs_unlock();
        gamepak_gs_send_command(0x9090);
        sleep_us(50);

        adbus_set_direction(false);
        adbus_latch_address(N64_GS_ID_ADDR);
        uint16_t mfg_id = adbus_read_word();
        adbus_latch_address(N64_GS_ID_ADDR + 2u);
        uint16_t dev_id = adbus_read_word();

        // Some boards can return words in reverse order; accept known-ID fallback.
        if (!gs_is_known_device_id(dev_id) && gs_is_known_device_id(mfg_id)) {
            uint16_t tmp = mfg_id;
            mfg_id = dev_id;
            dev_id = tmp;
            flags |= N64_GS_FLAG_ID_WORDS_SWAPPED;
        }

        final_mfg = mfg_id;
        final_dev = dev_id;

        if (gs_is_known_device_id(dev_id) && !gs_word_looks_open_bus(mfg_id)) {
            break;
        }
        sleep_us(200);
    }

    if (gs_word_looks_open_bus(final_mfg) || gs_word_looks_open_bus(final_dev)) {
        flags |= N64_GS_FLAG_OPEN_BUS_ID;
    }
    if (!gs_is_known_device_id(final_dev)) {
        flags |= N64_GS_FLAG_UNKNOWN_DEVICE;
    }

    out_info->mfg_id = final_mfg;
    out_info->flash_id = final_dev;
    out_info->base_addr = N64_GS_ROM_BASE;
    out_info->flags = flags;
    out_info->chip_family = 1;
    out_info->caps = N64_GS_CAP_EXPORT | N64_GS_CAP_IMPORT | N64_GS_CAP_ERASE;
    out_info->size_bytes = gs_size_for_device_id(final_dev);
    out_info->present = (out_info->size_bytes != 0u) ? 1u : 0u;

    gamepak_gs_reset_to_read_mode(final_dev);
    return out_info->present != 0u;
}

bool gamepak_gs_read_bytes(uint32_t offset, uint8_t* buffer, size_t length) {
    if (!buffer || (offset & 1u) != 0u || (length & 1u) != 0u) {
        return false;
    }

    for (size_t i = 0; i < length; i += 2) {
        adbus_latch_address(N64_GS_ROM_BASE + offset + i);
        uint16_t word = adbus_read_word();
        buffer[i] = (uint8_t)(word >> 8);
        buffer[i + 1] = (uint8_t)(word & 0xFF);
    }

    return true;
}

static void gs_0404_unprotect(void) {
    adbus_set_direction(false);
    adbus_latch_address(0x1EF03044u); adbus_read_word();
    adbus_latch_address(0x1EE03040u); adbus_read_word();
    adbus_latch_address(0x1EE03044u); adbus_read_word();
    adbus_latch_address(0x1EE00830u); adbus_read_word();
    adbus_latch_address(0x1EF00834u); adbus_read_word();
    adbus_latch_address(0x1EF00830u); adbus_read_word();
    adbus_latch_address(0x1EE00834u); adbus_read_word();
    sleep_ms(1000);
}

static void gs_0404_protect(void) {
    adbus_set_direction(false);
    adbus_latch_address(0x1EF03044u); adbus_read_word();
    adbus_latch_address(0x1EE03040u); adbus_read_word();
    adbus_latch_address(0x1EE03044u); adbus_read_word();
    adbus_latch_address(0x1EE00830u); adbus_read_word();
    adbus_latch_address(0x1EF00834u); adbus_read_word();
    adbus_latch_address(0x1EF00830u); adbus_read_word();
    adbus_latch_address(0x1EE00814u); adbus_read_word();
    sleep_ms(1000);
}

bool gamepak_gs_erase(uint16_t flash_id) {
    if (flash_id == N64_GS_DEV_ID_SST28LF040) {
        gs_0404_unprotect();
        gamepak_gs_send_command(0x3030);
        adbus_set_direction(true);
        adbus_latch_address(N64_GS_CMD_ADDR_1);
        adbus_write_word(0x3030);
        adbus_set_direction(false);
        sleep_ms(1000);
        return true;
    } else if (flash_id == N64_GS_DEV_ID_SST29LE010 || flash_id == N64_GS_DEV_ID_AT29LV010A || flash_id == N64_GS_DEV_ID_SST29EE010 || flash_id == N64_GS_DEV_ID_UNKNOWN_1208) {
        gamepak_gs_send_command(0x8080);
        gamepak_gs_send_command(0x1010);
        sleep_ms(20);
        return true;
    }
    return false;
}

bool gamepak_gs_write_bytes(uint16_t flash_id, uint32_t offset, const uint8_t* buffer, size_t length) {
    if (!buffer || (offset & 1u) != 0u || (length & 1u) != 0u) {
        return false;
    }

    if (flash_id == N64_GS_DEV_ID_SST28LF040) {
        // SST 28LF040 byte program
        for (size_t i = 0; i < length; i += 2) {
            gamepak_gs_send_command(0x1010);

            uint16_t word = ((uint16_t)buffer[i] << 8) | buffer[i + 1];
            adbus_set_direction(true);
            adbus_latch_address(N64_GS_ROM_BASE + offset + i);
            adbus_write_word(word);
            adbus_set_direction(false);

            sleep_us(60);
        }
        gs_0404_protect();
        return true;
    } else if (flash_id == N64_GS_DEV_ID_SST29LE010 || flash_id == N64_GS_DEV_ID_AT29LV010A || flash_id == N64_GS_DEV_ID_SST29EE010 || flash_id == N64_GS_DEV_ID_UNKNOWN_1208) {
        // 128 byte pages per chip, so 256 bytes per page for the parallel pair
        for (size_t i = 0; i < length; i += 256) {
            size_t page_len = length - i;
            if (page_len > 256) {
                page_len = 256;
            }

            gamepak_gs_send_command(0xA0A0);

            adbus_set_direction(true);
            for (size_t j = 0; j < page_len; j += 2) {
                uint16_t word = ((uint16_t)buffer[i + j] << 8) | buffer[i + j + 1];
                adbus_latch_address(N64_GS_ROM_BASE + offset + i + j);
                adbus_write_word(word);
            }
            adbus_set_direction(false);

            sleep_ms(30);
        }
        return true;
    }

    return false;
}
