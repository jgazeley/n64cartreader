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

bool joybus_init(void);
void joybus_reset(void);

size_t joybus_get_eeprom_size(void);
bool joybus_read_eeprom_block(uint8_t block_index, uint8_t* buffer);
bool joybus_write_eeprom_block(uint8_t block_index, const uint8_t data[8]);



#endif // N64_JOYBUS_H
