#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/structs/sio.h"
#include "hardware/gpio.h"

#include <bus/ad_bus.h>

// ======================================================================
// N64 Cartridge Hardware Definitions & Pin Assignments (Definitions)
// Purpose: Define the actual values for constants declared in ad_bus.h.
// ======================================================================

void n64_reset() {
    gpio_put(RST_PIN, 0);
    sleep_ms(20);
    gpio_put(RST_PIN, 1);
    sleep_ms(150);
}

// N64 Bus Communication Functions
void n64_adBus_init() {

    // RST_PIN setup: Initially HIGH (inactive)
    gpio_init(RST_PIN);
    gpio_set_dir(RST_PIN, GPIO_OUT);
    gpio_put(RST_PIN, 1);

    // Control pins setup: Initially HIGH (inactive)
    const uint8_t control_pins[] = {WR_PIN, RD_PIN, ALE_H_PIN, ALE_L_PIN};
    for (size_t i = 0; i < sizeof(control_pins) / sizeof(control_pins[0]); ++i) {
        uint8_t p = control_pins[i];
        gpio_init(p);
        gpio_set_dir(p, GPIO_OUT);
        gpio_put(p, 1); // Set these pins HIGH to their inactive state
    }

    // --- Initialize AD bus pins to input with pullups ---
    adBus_dir(false);

    // --- Perform N64 cartridge hardware reset sequence ---
    n64_reset();
}

void adBus_dir(bool out) {
  if (out) { // Configure for OUTPUT (Pico drives the bus)
    // First, ensure all AD bus pins are explicitly set to GPIO_FUNC_SIO
    // and have their pulls disabled, as is standard for digital outputs.
    for (int i = 0; i < AD_BUS_PIN_COUNT; ++i) {
        uint current_pin = AD_BUS_PIN_START + i;
        gpio_set_function(current_pin, GPIO_FUNC_SIO); // Set function to Software I/O.
        gpio_disable_pulls(current_pin);                // Disable pull-ups/downs for output.
    }
    // Set Output Enable for all pins in AD_BUS_MASK simultaneously using SIO.
    sio_hw->gpio_oe_set = AD_BUS_MASK;

    // Set the initial output value of these pins to LOW simultaneously using SIO.
    sio_hw->gpio_clr = AD_BUS_MASK;     // Sets output low for masked pins.

  } else { // Configure for INPUT with PULLUPS (Pico reads from the bus)
    // First, ensure all AD bus pins are set to GPIO_FUNC_SIO and have pull-ups enabled.
    // Pull-ups are important for stability when reading from a bus that might otherwise float.
    for (int i = 0; i < AD_BUS_PIN_COUNT; ++i) {
        uint current_pin = AD_BUS_PIN_START + i;
        gpio_set_function(current_pin, GPIO_FUNC_SIO); // Set function to Software I/O.
        gpio_pull_up(current_pin);                      // Enable internal pull-up resistor.
    }
    // Clear Output Enable for all pins in AD_BUS_MASK simultaneously using SIO.
    // This makes all specified AD bus pins function as inputs.
    sio_hw->gpio_oe_clr = AD_BUS_MASK;
  }
}

// number of NOPs for a ~56 ns latch delay
#define LATCH_NOPS     7
// NOPs for the ~32 ns bus turnaround
#define TURNAROUND_NOPS 4
// mask of all four control lines in their inactive (HIGH) state
#define CTRL_INACTIVE_MASK  \
   ((1UL<<WR_PIN)|(1UL<<RD_PIN)|(1UL<<ALE_H_PIN)|(1UL<<ALE_L_PIN))

// helper to drive a 16-bit word onto AD0–15 and pulse ALE↓
static inline void adBus_latch_word(uint16_t word, uint32_t ale_mask) {
    uint32_t v = (uint32_t)word << AD_BUS_PIN_START;
    sio_hw->gpio_clr = AD_BUS_MASK;   // clear bus
    sio_hw->gpio_set = v;            // set new value
    critical_delay_nops(LATCH_NOPS); // setup time
    sio_hw->gpio_clr = ale_mask;     // pulse ALE low
    critical_delay_nops(LATCH_NOPS); // hold time
}

void adBus_set_address(uint32_t addr) {
    uint16_t hi = addr >> 16;
    uint16_t lo = (uint16_t)addr;

    // 1) make sure no bus cycles happen while we’re setting up
    sio_hw->gpio_set = CTRL_INACTIVE_MASK;

    // 2) drive AD0–15
    adBus_dir(true);

    // 3) latch high then low half
    adBus_latch_word(hi, (1UL<<ALE_H_PIN));
    adBus_latch_word(lo, (1UL<<ALE_L_PIN));

    // 4) release bus to the cartridge
    adBus_dir(false);
    critical_delay_nops(TURNAROUND_NOPS);
}

uint16_t n64_read16() {
  sio_hw->gpio_clr = (1UL << RD_PIN); // Assert /RD (drive RD_PIN LOW) to initiate the read cycle.

  // Wait for N64 Read Access Time (T_acs(RD) max ~440ns for ROM).
  // 55 NOPs * 8ns/NOP = 440ns. This is the critical delay from /RD going low
  // until data is guaranteed to be valid on the bus.
  critical_delay_nops(55);

  uint32_t port_val = sio_hw->gpio_in; // Read the state of all GPIO pins at once.
  // Extract the 16 bits corresponding to the AD bus (AD_BUS_MASK) from the full
  // 32-bit `gpio_in` register value. Then, shift these bits down so that the
  // bit from AD0 (AD_BUS_PIN_START) is at bit 0 of the `v` result.
  uint16_t v = (uint16_t)((port_val & AD_BUS_MASK) >> AD_BUS_PIN_START);

  sio_hw->gpio_set = (1UL << RD_PIN); // De-assert /RD (drive RD_PIN HIGH) to end the read cycle.
  critical_delay_nops(7); // Data Hold Time (T_h(RD-AD) min ~30ns for N64).
                          // 7 NOPs = 56ns, ensuring the cartridge keeps data valid briefly.
  return v;
}

uint16_t sram_read_word(uint32_t addr) {
    // 1) Drive address onto AD[0..15] and strobe ALE_H/ALE_L
    adBus_set_address(addr);
    // 2) Assert RD, sample data bus, release RD
    return n64_read16();
}

// --- Write one 16-bit word: place on bus, assert WR low for ~440 ns, release ---
//    (This parallels readWord_SIO but driving WR instead of RD.)
void writeWord_SIO(uint16_t data) {
  // Drive AD bus as output
  adBus_dir(true);

  uint32_t data_bits = ((uint32_t)data << AD_BUS_PIN_START) & AD_BUS_MASK;
  sio_hw->gpio_clr = AD_BUS_MASK;    // clear old bits
  sio_hw->gpio_set = data_bits;      // drive new data
  critical_delay_nops(7);

  // Pulse WR low/high
  sio_hw->gpio_clr = (1UL << WR_PIN);
  critical_delay_nops(55);
  sio_hw->gpio_set = (1UL << WR_PIN);
  critical_delay_nops(7);

  // Float bus again
  adBus_dir(false);
}

// --- Write the first 32 bytes of SRAM with the pattern DE AD BE EF repeated ---
//    => that's 16 words: {0xDEAD, 0xBEEF, 0xDEAD, ...}
void write_first_32_bytes() {
  bool writeDEAD = true;
  for (uint32_t offset = 0; offset < 32; offset += 2) {
    uint32_t curr_addr = N64_SRAM_BASE + offset;
    adBus_set_address(curr_addr);

    uint16_t word_to_write = writeDEAD ? 0xDEAD : 0xBEEF;
    writeWord_SIO(word_to_write);

    writeDEAD = !writeDEAD;
  }
  printf("SRAM write complete.");
}

#define SRAM_SIZE_BYTES   (32 * 1024u)
#define SRAM_CHUNK_SIZE   256u   // bytes per chunk

static uint8_t sram_chunk_buffer[SRAM_CHUNK_SIZE];

void dump_sram_to_stdio(void) {
    printf("Starting SRAM dump (HEX ONLY)...\n");
    size_t bytes_on_line = 0;

    for (uint32_t chunk_off = 0; chunk_off < SRAM_SIZE_BYTES; chunk_off += SRAM_CHUNK_SIZE) {
        // Read one 256-byte chunk (128 words)
        for (uint32_t word_off = 0; word_off < SRAM_CHUNK_SIZE; word_off += 2) {
            uint32_t addr = N64_SRAM_BASE + chunk_off + word_off;
            uint16_t w   = sram_read_word(addr);
            // store big-endian
            sram_chunk_buffer[word_off    ] = (uint8_t)(w >> 8);
            sram_chunk_buffer[word_off + 1] = (uint8_t)(w & 0xFF);
        }
        // Print chunk as hex, 16 bytes per line
        for (uint32_t i = 0; i < SRAM_CHUNK_SIZE; ++i) {
            printf("%02X", sram_chunk_buffer[i]);
            if (++bytes_on_line >= 16) {
                printf("\n");
                bytes_on_line = 0;
            }
        }
    }
    if (bytes_on_line) {
        printf("\n");
    }
    printf("SRAM dump complete.\n");
}

void critical_delay_nops(int nops_count) {
  for (int i = 0; i < nops_count; ++i) {
    __asm volatile("nop\n"); // Inline assembly for a single NOP instruction.
  }
}