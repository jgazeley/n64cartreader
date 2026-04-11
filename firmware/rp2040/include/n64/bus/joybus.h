/**
 * @file joybus.h
 * @brief Joybus (controller port) interface for N64.
 *
 * Centralizes Joybus initialization, reset, controller polling,
 * and Memory Pak operations. Leverages PIO/SIO for precise timing.
 */
#ifndef N64_JOYBUS_H
#define N64_JOYBUS_H

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void InitEeprom(uint dataPin);
void InitEepromClock(uint clockpin);
void ReadEepromData(uint32_t offset, uint8_t *buffer);
void WriteEepromData(uint32_t offset, uint8_t *buffer);

typedef enum {
    JOYBUS_MODE_OFF = 0,
    JOYBUS_MODE_CART_EEPROM = 1,
    JOYBUS_MODE_CONTROLLER = 2,
} joybus_mode_t;

bool joybus_init(void);
void joybus_deinit(void);
void joybus_reset(void);
bool joybus_set_mode(joybus_mode_t mode);
joybus_mode_t joybus_get_mode(void);

void joybus_set_eeprom_size_hint(size_t size_bytes);
size_t joybus_get_eeprom_size(void);
bool joybus_had_probe_collision(void);
bool joybus_read_eeprom_block(uint8_t block_index, uint8_t* buffer);
bool joybus_write_eeprom_block(uint8_t block_index, const uint8_t data[8]);

/**
 * @brief Send one Joybus message and receive a fixed-size response.
 *
 * This is a low-level helper used by controller and future mempak operations.
 *
 * @param tx Command bytes to send
 * @param tx_len Number of command bytes
 * @param rx Destination buffer for response bytes
 * @param rx_len Number of response bytes expected
 * @param timeout_us Timeout waiting for first response byte
 * @param retries Number of retries if the first response byte times out
 * @return true on success
 */
bool joybus_transfer(
    const uint8_t *tx,
    size_t tx_len,
    uint8_t *rx,
    size_t rx_len,
    uint32_t timeout_us,
    uint32_t retries
);

#endif // N64_JOYBUS_H
