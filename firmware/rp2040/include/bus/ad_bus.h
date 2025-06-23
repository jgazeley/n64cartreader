/* ad_bus.h */
#ifndef AD_BUS_H_
#define AD_BUS_H_

// ======================================================================
// N64 Cartridge Hardware Definitions & Pin Assignments
// Purpose: Declare constants and pin mappings for the N64 cartridge
//          interface on the Raspberry Pi Pico. These are visible globally
//          to any file that includes ad_bus.h.
// ======================================================================

// Base memory address of the N64 cartridge ROM
#define N64_ROM_BASE             0x10000000u
#define N64_SRAM_BASE            0x08000000u

// Control pins
#define AD0                  0     // First GPIO of the 16-bit AD bus
#define ALE_H_PIN            19
#define ALE_L_PIN            20
#define RD_PIN               18
#define WR_PIN               17
#define RST_PIN              16

// Width of the AD bus
#define AD_BUS_PIN_START     AD0
#define AD_BUS_PIN_COUNT     16

// Bitmask for GPIO AD0–AD15: (1<<16)-1 shifted by start
#define AD_BUS_MASK          (((1u << AD_BUS_PIN_COUNT) - 1) << AD_BUS_PIN_START)

// Declare the functions that interact directly with the N64 cartridge bus.
void n64_reset();
void n64_adBus_init();
void adBus_dir(bool out);
void adBus_set_address(uint32_t addr);
uint16_t n64_read16();
uint16_t sram_read_word(uint32_t addr);
void dump_sram_to_stdio(void);
void write_first_32_bytes();
void critical_delay_nops(int nops_count);

#endif // AD_BUS_H_