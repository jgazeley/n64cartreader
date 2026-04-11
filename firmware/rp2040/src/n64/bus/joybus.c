//Command Description   Console Devices  Tx Bytes Rx Bytes
//0xFF    Reset & info  N64 Cartridge    1        3
//0x04    Read EEPROM   N64 Cartridge    2        8
//0x05    Write EEPROM  N64 Cartridge    10       1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/platform.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"

#include "n64/pins.h"
#include "n64/bus/joybus.h"
#include "n64/bus/generated/joybus.pio.h"

// Headless mode: keep serial stream binary-clean for raw host protocol.
#define printf(...) ((void)0)

uint32_t ReadCount = 0;
uint32_t gEepromSize = 0;
static bool s_eeprom_probe_collision = false;
static bool s_joybus_initialized = false;
static joybus_mode_t s_joybus_mode = JOYBUS_MODE_OFF;
static uint s_clock_pio_offset = 0;

PIO pio = pio0;
PIO pio_1 = pio1;
pio_sm_config config;
uint piooffset;

static uint32_t GetInputWithTimeoutUs(uint32_t timeout_us);
static void joybus_init_data_sm(uint dataPin);

void joybus_reset() {
    // Pulse the N64 system reset: assert LOW, then release HIGH.
    // Required for repro carts with CPLD/FPGA bridges that need a clean
    // reset before they will respond on the Joybus.
    gpio_put(N64_SYSTEM_RESET_PIN, false);
    sleep_ms(300);
    gpio_put(N64_SYSTEM_RESET_PIN, true);
    sleep_ms(100);
}

void joybus_deinit(void) {
    if (!s_joybus_initialized) return;

    pio_sm_set_enabled(pio, 0, false);
    pio_sm_set_enabled(pio_1, 1, false);
    pio_sm_clear_fifos(pio, 0);
    pio_sm_clear_fifos(pio_1, 1);

    pio_remove_program(pio, &joybus_program, piooffset);
    pio_remove_program(pio_1, &joybus_program, s_clock_pio_offset);

    gpio_set_function(N64_EEPROM_DATA_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(N64_EEPROM_DATA_PIN, GPIO_IN);
    gpio_pull_up(N64_EEPROM_DATA_PIN);

    gpio_set_function(N64_EEPROM_CLOCK_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(N64_EEPROM_CLOCK_PIN, GPIO_IN);

    s_joybus_initialized = false;
    s_joybus_mode = JOYBUS_MODE_OFF;
    gEepromSize = 0;
    ReadCount = 0;
    s_eeprom_probe_collision = false;
}

bool joybus_init() {
    if (s_joybus_initialized) {
        joybus_deinit();
    }

    InitEepromClock(N64_EEPROM_CLOCK_PIN);
    joybus_reset();
    sleep_ms(50);

    // Host-driven save architecture: initialize Joybus transport only.
    // EEPROM size/type is now provided explicitly by the host profile.
    joybus_init_data_sm(N64_EEPROM_DATA_PIN);
    gEepromSize = 0;
    ReadCount = 0;
    s_eeprom_probe_collision = false;

    s_joybus_initialized = true;
    s_joybus_mode = JOYBUS_MODE_CART_EEPROM;
    return true;
}

bool joybus_set_mode(joybus_mode_t mode)
{
    switch (mode) {
        case JOYBUS_MODE_OFF:
        case JOYBUS_MODE_CART_EEPROM:
        case JOYBUS_MODE_CONTROLLER:
            break;
        default:
            return false;
    }

    // A2 scaffold: allow pre-init ownership intent to be recorded.
    // Hardware-level mode transitions are deferred to A3.
    s_joybus_mode = mode;
    return true;
}

joybus_mode_t joybus_get_mode(void)
{
    return s_joybus_mode;
}

void __time_critical_func(convertToPio)(const uint8_t* command, const int len, uint32_t* result, int* resultLen) {
    if (len == 0) {
        *resultLen = 0;
        return;
    }
    *resultLen = len/2 + 1;
    int i;
    for (i = 0; i < *resultLen; i++) {
        result[i] = 0;
    }
    for (i = 0; i < len; i++) {
        for (int j = 0; j < 8; j++) {
            result[i / 2] += (uint32_t)(1 << (2 * (8 * (i % 2) + j) + 1));
            result[i / 2] += (uint32_t)((!!(command[i] & (0x80u >> j))) << (2 * (8 * (i % 2) + j)));
        }
    }
    // End bit
    result[len / 2] += 3 << (2 * (8 * (len % 2)));
}

void __time_critical_func(InitEepromClock)(uint clockpin)
{
    gpio_init(clockpin);
    gpio_set_dir(clockpin, GPIO_OUT);

    pio_gpio_init(pio_1, clockpin);

    s_clock_pio_offset = (uint)pio_add_program(pio_1, &joybus_program);
    pio_sm_config config1 = joybus_program_get_default_config(s_clock_pio_offset);
    //sm_config_set_out_pins(&config1, clockpin, 1);
    sm_config_set_set_pins(&config1, clockpin, 1);
    sm_config_set_clkdiv(&config1, 5);
    //sm_config_set_out_shift(&config1, true, false, 32);
    //sm_config_set_in_shift(&config1, false, true, 8);

    pio_sm_init(pio_1, 1, s_clock_pio_offset + joybus_offset_clockgen, &config1);
    pio_sm_set_enabled(pio_1, 1, true);
}

static void joybus_init_data_sm(uint dataPin)
{
    gpio_init(dataPin);
    gpio_set_dir(dataPin, GPIO_IN);
    gpio_pull_up(dataPin);

    sleep_us(100); // Stabilize voltages

    pio_gpio_init(pio, dataPin);

    piooffset = (uint)pio_add_program(pio, &joybus_program);
    config = joybus_program_get_default_config(piooffset);
    sm_config_set_in_pins(&config, dataPin);
    sm_config_set_out_pins(&config, dataPin, 1);
    sm_config_set_set_pins(&config, dataPin, 1);
    sm_config_set_clkdiv(&config, 5);
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_in_shift(&config, false, true, 8);

    pio_sm_init(pio, 0, piooffset, &config);
    pio_sm_set_enabled(pio, 0, true);
}

static uint32_t GetInputWithTimeoutUs(uint32_t timeout_us)
{
    uint32_t lastWriteTime = time_us_32();
    while (1) {
        if(pio_sm_is_rx_fifo_empty(pio, 0)) {
            uint32_t now = time_us_32();
            uint32_t diff = now - lastWriteTime;

            // Send the eeprom data if it's been ?Seconds since the last eeprom write
            // Reset the lastWriteTime to 0 and don't sent the data unless we get another write
            if (lastWriteTime != 0 && diff > timeout_us) {
                lastWriteTime = 0;
                break;
            }
        } else {
            return pio_sm_get(pio, 0);
        }
    }

    return 0xFFFFFFFF;
}

uint32_t GetInputWithTimeout(void)
{
    // Historical default used by EEPROM paths.
    return GetInputWithTimeoutUs(1000);
}

void __time_critical_func(InitEeprom)(uint dataPin)
{
    gEepromSize = 0;
    ReadCount = 0;
    s_eeprom_probe_collision = false;
    joybus_init_data_sm(dataPin);

    // Send the info command
    {
        uint8_t probeResponse[1] = {0x00};
        uint32_t result[8];
        int resultLen;
        convertToPio(probeResponse, 1, result, &resultLen);

        pio_sm_set_enabled(pio, 0, false);
        pio_sm_init(pio, 0, piooffset + joybus_offset_outmode, &config);
        pio_sm_set_enabled(pio, 0, true);

        for (int i = 0; i < resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);
    }

    // Check response
    uint32_t buffer[3];
    buffer[0] = GetInputWithTimeout();
    printf("DEBUG joybus: buffer[0] = 0x%08X\n", (unsigned)buffer[0]);

    if (buffer[0] == 0) {
        buffer[1] = pio_sm_get_blocking(pio, 0);
        buffer[2] = pio_sm_get_blocking(pio, 0);
        printf("DEBUG joybus: buffer[1] = 0x%08X, buffer[2] = 0x%08X\n",
               (unsigned)buffer[1], (unsigned)buffer[2]);

        // Determine the size of the EEPROM.
        if (buffer[1] == 0x80) {
            // 4K Eeprom.
            ReadCount = 64;
            gEepromSize = 512;
        } else if (buffer[1] == 0xC0) {
            // 16K Eeprom.
            ReadCount = 256;
            gEepromSize = 512 * 4;
        } else if (buffer[1] == 0x00 && buffer[2] == 0x00) {
            // Shared Joybus line collision case:
            // Cartridge EEPROM and controller can both respond to 0x00 (info),
            // resulting in bitwise contention and a 0x00/0x00/0x00 pattern.
            // Keep EEPROM size at 0 and surface this via a diagnostic flag.
            s_eeprom_probe_collision = true;
        } else {
            // Unknown SI eeprom type.
            ReadCount = 0;
            gEepromSize = 0;
        }
    }
}

void __time_critical_func(ReadEepromData)(uint32_t offset, uint8_t *buffer)
{
    if (gEepromSize == 0) {
        return;
    }

    // Read the eeprom.
    for (uint32_t ReadIndex = 0; ReadIndex < 64; ReadIndex += 1) {
        // Construct the read command.
        uint8_t probeResponse[] = {0x04, (uint8_t)(ReadIndex + offset)};
        uint32_t result[8];
        int resultLen;
        convertToPio(probeResponse, 2, result, &resultLen);

        uint32_t firstInput;
        uint32_t retries = 0;
        do {
            // Send the read command
            pio_sm_set_enabled(pio, 0, false);
            pio_sm_init(pio, 0, piooffset + joybus_offset_outmode, &config);
            pio_sm_set_enabled(pio, 0, true);

            for (int i = 0; i < resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);

            firstInput = GetInputWithTimeout();
            if (retries > 10) {
                gEepromSize = 0;
                return;
            }
            retries += 1;

        } while (firstInput == 0xFFFFFFFF);
        // Read the incoming data from the cart.
        buffer[(uint)ReadIndex * 8] = (uint8_t)firstInput;
        for (int i = 1; i < 8; i += 1) {
            buffer[(uint)i + (uint)ReadIndex * 8] = (uint8_t)pio_sm_get_blocking(pio, 0);
        }
        sleep_us(200);
    }
}

void __time_critical_func(WriteEepromData)(uint32_t offset, uint8_t *buffer)
{
    // Write the eeprom.
    for (uint32_t WriteIndex = 0; WriteIndex < 64; WriteIndex += 1) {
        // Construct the write command.
        uint8_t probeResponse[10] = {0x05, (uint8_t)(WriteIndex + offset)};
        for (uint i = 0; i < 8; i += 1) {
            probeResponse[i + 2] = buffer[i + (WriteIndex * 8)];
        }

        uint32_t result[10];
        int resultLen;
        convertToPio(probeResponse, 10, result, &resultLen);

        uint32_t firstInput;
        uint32_t retries = 0;
        do {
            // Send the read command
            pio_sm_set_enabled(pio, 0, false);
            pio_sm_init(pio, 0, piooffset + joybus_offset_outmode, &config);
            pio_sm_set_enabled(pio, 0, true);

            for (int i = 0; i < resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);

            firstInput = GetInputWithTimeout();
            if (retries > 10) {
                gEepromSize = 0;
                return;
            }
            retries += 1;

        } while (firstInput == 0xFFFFFFFF);
        // Read the incoming data from the cart.
        uint8_t response[2];
        response[0] = (uint8_t)firstInput;
        if (response[0] != 0) {
            sleep_ms(10);
        }

        sleep_us(200);
    }
}


void joybus_set_eeprom_size_hint(size_t size_bytes)
{
    if (size_bytes == 512u) {
        ReadCount = 64;
        gEepromSize = 512;
    } else if (size_bytes == 2048u) {
        ReadCount = 256;
        gEepromSize = 2048;
    } else {
        ReadCount = 0;
        gEepromSize = 0;
    }
    s_eeprom_probe_collision = false;

    // Send Joybus reset (0xFF) + info query (0x00) preamble when EEPROM is
    // configured.  Retail EEPROM chips tolerate skipping this, but repro carts
    // with an FPGA/CPLD bridge may require the handshake before they will
    // translate read/write commands.
    if (gEepromSize > 0 && s_joybus_initialized) {
        uint8_t rx[3];
        const uint8_t reset_cmd[] = { 0xFF };
        joybus_transfer(reset_cmd, 1, rx, 3, 5000u, 3u);
        sleep_ms(10);
        const uint8_t info_cmd[] = { 0x00 };
        joybus_transfer(info_cmd, 1, rx, 3, 5000u, 3u);
        sleep_ms(10);
    }
}

size_t joybus_get_eeprom_size(void) {
    // Size is host-configured via joybus_set_eeprom_size_hint() in headless mode.
    extern uint32_t gEepromSize;
    return gEepromSize;
}

bool joybus_had_probe_collision(void)
{
    return s_eeprom_probe_collision;
}


bool joybus_read_eeprom_block(uint8_t block_index, uint8_t* buffer) {
    if (!buffer) return false;

    const uint8_t read_command[] = { 0x04, block_index };
    return joybus_transfer(read_command, sizeof(read_command), buffer, 8, 3000u, 20u);
}

bool joybus_write_eeprom_block(uint8_t block_index, const uint8_t data[8])
{
    if (!data) return false;

    // ----- 1. Build the 0x05 command frame -----
    uint8_t cmd[10] = { 0x05, block_index };
    memcpy(&cmd[2], data, 8);

    uint8_t ack = 0xFF;
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (joybus_transfer(cmd, sizeof(cmd), &ack, 1, 5000u, 1u) && ack == 0x00) {
            return true;
        }
        sleep_ms(10);
    }

    return false; // exceeded retries
}

bool joybus_transfer(
    const uint8_t *tx,
    size_t tx_len,
    uint8_t *rx,
    size_t rx_len,
    uint32_t timeout_us,
    uint32_t retries
)
{
    if (!tx || tx_len == 0 || tx_len > 64) {
        return false;
    }
    if (rx_len > 0 && !rx) {
        return false;
    }
    if (retries == 0) {
        retries = 1;
    }

    uint32_t frame[33];
    int frame_len = 0;
    convertToPio(tx, (int)tx_len, frame, &frame_len);

    for (uint32_t attempt = 0; attempt < retries; ++attempt) {
        pio_sm_set_enabled(pio, 0, false);
        pio_sm_init(pio, 0, piooffset + joybus_offset_outmode, &config);
        pio_sm_set_enabled(pio, 0, true);

        for (int i = 0; i < frame_len; ++i) {
            pio_sm_put_blocking(pio, 0, frame[i]);
        }

        if (rx_len == 0) {
            sleep_us(200);
            return true;
        }

        uint32_t first = GetInputWithTimeoutUs(timeout_us);
        if (first == 0xFFFFFFFF) {
            continue;
        }

        rx[0] = (uint8_t)first;
        for (size_t i = 1; i < rx_len; ++i) {
            uint32_t next = GetInputWithTimeoutUs(timeout_us);
            if (next == 0xFFFFFFFF) {
                goto retry_transfer;
            }
            rx[i] = (uint8_t)next;
        }

        sleep_us(200);
        return true;
retry_transfer:
        ;
    }

    return false;
}

