/**
 * @file adbus.h
 * @brief Low-level 16-bit multiplexed Address/Data bus interface for N64 cartridges.
 *
 * The N64 cartridge bus time-multiplexes A0–A15 (high address phase) then
 * D0–D15 (read/write data phase) on the same 16 GPIO lines. ALE_L and ALE_H
 * strobes latch the address, and RD/WR strobes control read/write cycles.
 *
 * This module abstracts:
 * - GPIO configuration for the 16-bit bus.
 * - Address latching via ALE_H/ALE_L.
 * - Word-wide reads and writes via /RD and /WR strobes.
 * - Cartridge reset control.
 *
 * This must be initialized once (adbus_init()) before any gamepak operations.
 */
#ifndef N64_BUS_ADBUS_H
#define N64_BUS_ADBUS_H

#include <stdint.h>
#include <stdbool.h>
#include "n64/pins.h"

void adbus_set_direction(bool output);

/**
 * @brief Initialize the AD-bus GPIOs and control lines.
 *
 * Configures:
 * - GPIOs [N64_ADBUS_PIN_START .. +15] for bidirectional A/D.
 * - N64_ADBUS_WR_PIN, N64_ADBUS_RD_PIN, N64_ADBUS_ALE_H_PIN,
 * N64_ADBUS_ALE_L_PIN, and N64_SYSTEM_RESET_PIN as outputs.
 *
 * @return true on success.
 */
bool adbus_init(void);

/**
 * @brief Drive a 32-bit address onto the bus and latch it.
 *
 * Sequence:
 * 1. Drive high 16 bits (A16–A31), pulse ALE_H.
 * 2. Drive low 16 bits (A0–A15), pulse ALE_L.
 *
 * @param addr The full 32-bit address to be latched.
 */
void adbus_latch_address(uint32_t addr);

/**
 * @brief Read one 16-bit word from the bus.
 *
 * Asserts N64_ADBUS_RD_PIN (active-low), samples the 16 GPIOs, then releases.
 *
 * @return The latched 16-bit data from the cartridge.
 */
uint16_t adbus_read_word(void);

/**
 * @brief Write one 16-bit word to the bus.
 *
 * Drives the 16 GPIOs, asserts N64_ADBUS_WR_PIN (active-low), then releases.
 *
 * @param data The 16-bit data to write.
 */
void adbus_write_word(uint16_t data);

/**
 * @brief Assert or release the cartridge bus reset line.
 *
 * @param active true to assert reset (drive pin low), false to release (drive pin high).
 */
void adbus_assert_reset(bool active);

#endif // N64_BUS_ADBUS_H
