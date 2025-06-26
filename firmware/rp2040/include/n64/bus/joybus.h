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

void InitEeprom(uint dataPin);
void InitEepromClock(uint clockpin);
void ReadEepromData(uint32_t offset, uint8_t *buffer);
void WriteEepromData(uint32_t offset, uint8_t *buffer);
bool n64_joybus_init(void);
void n64_joybus_reset(void);

#endif // N64_JOYBUS_H
