/**
 * @file pins.h
 * @brief Hardware pin definitions for the N64 interface.
 *
 * This file centralizes all GPIO pin assignments for the project.
 * If you change the physical wiring, this is the only file you
 * should need to edit.
 */
#ifndef N64_PINS_H
#define N64_PINS_H

// --- Main N64 AD-Bus Data Pins (GPIO0 - GPIO15) ---
#define N64_ADBUS_PIN_START     0   // The first GPIO pin of the 16-bit bus
#define N64_ADBUS_PIN_COUNT     16
#define N64_ADBUS_GPIO_MASK     (((1u << N64_ADBUS_PIN_COUNT) - 1) << N64_ADBUS_PIN_START)

// --- Main N64 AD-Bus Control Pins ---
#define N64_SYSTEM_RESET_PIN    16  // Active-high reset for the N64 bus
#define N64_ADBUS_WR_PIN        17  // Write strobe (active-low)
#define N64_ADBUS_RD_PIN        18  // Read strobe (active-low)
#define N64_ADBUS_ALE_H_PIN     19  // Address latch enable high
#define N64_ADBUS_ALE_L_PIN     20  // Address latch enable low

// --- Peripheral Bus Pins ---
#define N64_CONTROLLER_DATA_PIN 21  // Data line for controllers (JoyBus)
#define N64_EEPROM_DATA_PIN     21  // Data line for cartridge EEPROM (can be shared)
#define N64_EEPROM_CLOCK_PIN    22  // Clock line for cartridge EEPROM

#endif // N64_PINS_H
