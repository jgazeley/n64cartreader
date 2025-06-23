/**
 * SPX-License-Identifier: BSD-2-Clause 
 * Copyright (c) 2023 - NopJne
 * 
 * Joybus
 * Provides SI joybus support for EEPROM interaction.
 */

#pragma once

// EEPROM pins
#define EEP_DAT              21
#define EEP_CLK              22
#define EEP_RST				 16

void n64_joyBus_reset();
void n64_eep_init();
void InitEeprom(uint dataPin);
void InitEepromClock(uint clockpin);
void ReadEepromData(uint32_t offset, uint8_t *buffer);
void WriteEepromData(uint32_t offset, uint8_t *buffer);

extern uint32_t gEepromSize;