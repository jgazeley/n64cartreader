#include "n64/devices/controller.h"

#include "n64/bus/joybus.h"
#include "pico/stdlib.h"

#include <string.h>

enum {
    N64_CMD_PROBE = 0x00,
    N64_CMD_RESET = 0xFF,
    N64_CMD_POLL = 0x01,
    N64_CMD_MEMPAK_READ = 0x02,
    N64_CMD_MEMPAK_WRITE = 0x03,
};

static bool s_initialized = false;
static bool s_connected = false;
static n64_controller_info_t s_info;
static n64_controller_state_t s_state;

static uint16_t mempak_addr_crc(uint16_t address)
{
    static const uint8_t xor_table[] = { 0x15, 0x1F, 0x0B, 0x16, 0x19, 0x07, 0x0E, 0x1C, 0x0D, 0x1A, 0x01 };
    uint8_t crc = 0;
    const uint8_t *cur = xor_table;
    for (uint16_t mask = 0x0020; mask; mask <<= 1, ++cur) {
        if (address & mask) {
            crc ^= *cur;
        }
    }
    return (uint16_t)((address & 0xFFE0u) | crc);
}

static uint8_t mempak_data_crc(const uint8_t data[32])
{
    uint8_t ret = 0;
    for (uint8_t i = 0; i <= 32; ++i) {
        for (uint8_t mask = 0x80; mask; mask >>= 1) {
            uint8_t tmp = (ret & 0x80u) ? 0x85u : 0;
            ret <<= 1;
            if (i < 32 && (data[i] & mask)) {
                ret |= 0x01u;
            }
            ret ^= tmp;
        }
    }
    return ret;
}

static bool ensure_controller_bus(void)
{
    if (s_initialized) {
        return true;
    }
    if (!joybus_init()) {
        return false;
    }
    s_initialized = true;
    return true;
}

static bool ensure_mempak_ready(void)
{
    if (!ensure_controller_bus()) {
        return false;
    }
    if (!s_connected && !controller_probe(NULL)) {
        return false;
    }
    return true;
}

static bool mempak_read_block_raw(uint16_t address, uint8_t out[32])
{
    const uint16_t addr_with_crc = mempak_addr_crc(address);
    uint8_t cmd[3] = {
        N64_CMD_MEMPAK_READ,
        (uint8_t)(addr_with_crc >> 8),
        (uint8_t)(addr_with_crc & 0xFF),
    };
    uint8_t rsp[33] = {0};
    if (!joybus_transfer(cmd, sizeof(cmd), rsp, sizeof(rsp), 5000u, 20u)) {
        return false;
    }
    memcpy(out, rsp, N64_MEMPAK_BLOCK_SIZE);
    return rsp[32] == mempak_data_crc(out);
}

static bool mempak_write_block_raw(uint16_t address, const uint8_t data[32])
{
    const uint16_t addr_with_crc = mempak_addr_crc(address);
    uint8_t cmd[3 + N64_MEMPAK_BLOCK_SIZE] = {0};
    cmd[0] = N64_CMD_MEMPAK_WRITE;
    cmd[1] = (uint8_t)(addr_with_crc >> 8);
    cmd[2] = (uint8_t)(addr_with_crc & 0xFF);
    memcpy(&cmd[3], data, N64_MEMPAK_BLOCK_SIZE);

    // Sanni flow: issue write command and don't rely solely on immediate ACK byte.
    if (!joybus_transfer(cmd, sizeof(cmd), NULL, 0, 5000u, 20u)) {
        return false;
    }

    // Give the pak time to commit, then verify by readback.
    sleep_us(1200);
    uint8_t verify[32] = {0};
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (mempak_read_block_raw(address, verify) && memcmp(verify, data, 32) == 0) {
            return true;
        }
        sleep_us(1500);
    }
    return false;
}

bool controller_init(void)
{
    if (!ensure_controller_bus()) {
        return false;
    }
    return controller_probe(NULL);
}

bool controller_probe(n64_controller_info_t *info)
{
    if (!ensure_controller_bus()) {
        s_connected = false;
        return false;
    }

    const uint8_t cmd[] = { N64_CMD_PROBE };
    uint8_t rsp[3] = {0};
    if (!joybus_transfer(cmd, sizeof(cmd), rsp, sizeof(rsp), 1000u, 10u)) {
        s_connected = false;
        return false;
    }

    s_info.device_id = (uint16_t)(((uint16_t)rsp[0] << 8) | rsp[1]);
    s_info.status = rsp[2];
    s_connected = (s_info.device_id != 0);

    if (info) {
        *info = s_info;
    }
    return s_connected;
}

bool controller_read(n64_controller_state_t *state, bool rumble)
{
    if (!state) {
        return false;
    }
    if (!s_connected && !controller_probe(NULL)) {
        return false;
    }

    // For baseline controller diagnostics, use simple 1-byte poll.
    // The 3-byte poll variant (mode + rumble) is reserved for future mempak/rumble work.
    uint8_t cmd[3] = { N64_CMD_POLL, 0x03, rumble ? 0x01 : 0x00 };
    size_t cmd_len = rumble ? 3u : 1u;
    uint8_t rsp[4] = {0};
    if (!joybus_transfer(cmd, cmd_len, rsp, sizeof(rsp), 1000u, 10u)) {
        s_connected = false;
        return false;
    }

    s_state.buttons = (uint16_t)(((uint16_t)rsp[0] << 8) | rsp[1]);
    s_state.stick_x = (int8_t)rsp[2];
    s_state.stick_y = (int8_t)rsp[3];
    *state = s_state;
    return true;
}

bool controller_is_connected(void)
{
    return s_connected;
}

bool controller_mempak_present(void)
{
    if (!ensure_mempak_ready()) {
        return false;
    }

    // Reset once before probing mempak path.
    const uint8_t reset_cmd[] = { N64_CMD_RESET };
    (void)joybus_transfer(reset_cmd, sizeof(reset_cmd), NULL, 0, 1000u, 3u);
    sleep_ms(20);
    if (!controller_probe(NULL)) {
        return false;
    }

    // Prefer an actual bus-level check over status byte heuristics.
    uint8_t block[32] = {0};
    return mempak_read_block_raw(0x0000, block);
}

bool controller_mempak_read_block(uint16_t address, uint8_t out[32])
{
    if (!out) {
        return false;
    }
    if ((address & (N64_MEMPAK_BLOCK_SIZE - 1u)) != 0u || address >= N64_MEMPAK_SIZE) {
        return false;
    }
    if (!ensure_mempak_ready()) {
        return false;
    }
    bool ok = mempak_read_block_raw(address, out);
    if (!ok) {
        s_connected = false;
    }
    return ok;
}

bool controller_mempak_write_block(uint16_t address, const uint8_t data[32])
{
    if (!data) {
        return false;
    }
    if ((address & (N64_MEMPAK_BLOCK_SIZE - 1u)) != 0u || address >= N64_MEMPAK_SIZE) {
        return false;
    }
    if (!ensure_mempak_ready()) {
        return false;
    }
    bool ok = mempak_write_block_raw(address, data);
    if (!ok) {
        s_connected = false;
    }
    return ok;
}

bool controller_mempak_read_all(uint8_t *out, size_t len)
{
    if (!out || len != N64_MEMPAK_SIZE) {
        return false;
    }
    if (!ensure_mempak_ready()) {
        return false;
    }

    for (uint16_t addr = 0; addr < N64_MEMPAK_SIZE; addr += N64_MEMPAK_BLOCK_SIZE) {
        if (!mempak_read_block_raw(addr, &out[addr])) {
            return false;
        }
        sleep_us(800);
    }
    return true;
}

bool controller_mempak_write_all(const uint8_t *data, size_t len)
{
    if (!data || len != N64_MEMPAK_SIZE) {
        return false;
    }
    if (!ensure_mempak_ready()) {
        return false;
    }

    for (uint16_t addr = 0; addr < N64_MEMPAK_SIZE; addr += N64_MEMPAK_BLOCK_SIZE) {
        if (!mempak_write_block_raw(addr, &data[addr])) {
            return false;
        }
        sleep_us(800);
    }
    return true;
}
