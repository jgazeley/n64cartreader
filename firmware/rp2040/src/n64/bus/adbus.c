/**
 * @file adbus.c
 * @brief Low-level 16-bit multiplexed Address/Data bus implementation for N64 cartridges.
 */
#include "n64/pins.h"
#include "n64/bus/adbus.h"

#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include "pico/stdlib.h"

// --- Private Constants ---
#define ADBUS_LATCH_DELAY_NOPS 7
#define ADBUS_TURNAROUND_NOPS 4
#define ADBUS_READ_ACCESS_NOPS 55
// #define ADBUS_READ_ACCESS_NOPS 120
#define ADBUS_WRITE_PULSE_NOPS 25

// Mask of all four control lines in their inactive (HIGH) state.
#define ADBUS_CTRL_INACTIVE_MASK \
    ((1UL << N64_ADBUS_WR_PIN) | (1UL << N64_ADBUS_RD_PIN) | (1UL << N64_ADBUS_ALE_H_PIN) | (1UL << N64_ADBUS_ALE_L_PIN))

// --- Private Helper Functions ---
static void adbus_delay_nops(int nops) {
    for (int i = 0; i < nops; ++i) {
        __asm volatile("nop\n");
    }
}

static inline void adbus_latch_word_internal(uint16_t word, uint32_t ale_pin_mask) {
    uint32_t v = (uint32_t)word << N64_ADBUS_PIN_START;
    
    // Place the word on the bus.
    sio_hw->gpio_clr = N64_ADBUS_GPIO_MASK;
    sio_hw->gpio_set = v;
    adbus_delay_nops(ADBUS_LATCH_DELAY_NOPS); // Address setup time

    // Pulse the corresponding ALE line low to latch the value.
    sio_hw->gpio_clr = ale_pin_mask;
    adbus_delay_nops(ADBUS_LATCH_DELAY_NOPS); // Hold time
    // sio_hw->gpio_set = ale_pin_mask;         // THE FIX: ALE goes HIGH again
    // The ALE line will be de-asserted by the gpio_set(CTRL_INACTIVE_MASK)
    // at the beginning of the next bus operation.
}

//==============================================================================
// Public API Function Implementations
//==============================================================================

void adbus_set_direction(bool output) {
    if (output) {
        // Set all AD bus pins to SIO function and disable pulls for output.
        for (int i = 0; i < N64_ADBUS_PIN_COUNT; ++i) {
            uint pin = N64_ADBUS_PIN_START + i;
            gpio_set_function(pin, GPIO_FUNC_SIO);
            gpio_disable_pulls(pin);
        }
        sio_hw->gpio_oe_set = N64_ADBUS_GPIO_MASK;
        sio_hw->gpio_clr = N64_ADBUS_GPIO_MASK; // Set bus low initially
    } else {
        // Set all AD bus pins to SIO function and enable pull-ups for input.
        for (int i = 0; i < N64_ADBUS_PIN_COUNT; ++i) {
            uint pin = N64_ADBUS_PIN_START + i;
            gpio_set_function(pin, GPIO_FUNC_SIO);
            gpio_pull_up(pin);
        }
        sio_hw->gpio_oe_clr = N64_ADBUS_GPIO_MASK;
    }
}


bool adbus_init(void) {
    const uint8_t control_pins[] = {
        N64_ADBUS_WR_PIN, N64_ADBUS_RD_PIN,
        N64_ADBUS_ALE_H_PIN, N64_ADBUS_ALE_L_PIN,
        N64_SYSTEM_RESET_PIN
    };

    for (size_t i = 0; i < sizeof(control_pins) / sizeof(control_pins[0]); ++i) {
        gpio_init(control_pins[i]);
        gpio_set_dir(control_pins[i], GPIO_OUT);
        gpio_put(control_pins[i], 1); // Set HIGH (inactive state for all lines)
    }

    adbus_set_direction(false);
    return true;
}


void adbus_latch_address(uint32_t addr) {
    uint16_t high_word = addr >> 16;
    uint16_t low_word = (uint16_t)addr;

    // Ensure all control lines are inactive before starting the sequence.
    sio_hw->gpio_set = ADBUS_CTRL_INACTIVE_MASK;

    // 1. Drive the AD bus from the Pico.
    adbus_set_direction(true);

    // 2. Latch the high 16 bits, then the low 16 bits.
    adbus_latch_word_internal(high_word, (1UL << N64_ADBUS_ALE_H_PIN));
    adbus_latch_word_internal(low_word, (1UL << N64_ADBUS_ALE_L_PIN));

    // 3. Release the bus back to the cartridge (set to input).
    adbus_set_direction(false);
    adbus_delay_nops(ADBUS_TURNAROUND_NOPS);
}


uint16_t adbus_read_word(void) {
    // Assert /RD (drive RD_PIN LOW) to begin the read cycle.
    sio_hw->gpio_clr = (1UL << N64_ADBUS_RD_PIN);
    adbus_delay_nops(ADBUS_READ_ACCESS_NOPS);

    // Read the state of all GPIO pins at once.
    uint32_t port_val = sio_hw->gpio_in;

    // De-assert /RD (drive RD_PIN HIGH) to end the read cycle.
    sio_hw->gpio_set = (1UL << N64_ADBUS_RD_PIN);
    adbus_delay_nops(ADBUS_LATCH_DELAY_NOPS);
    
    // Extract and shift the 16 bits corresponding to the AD bus.
    return (uint16_t)((port_val & N64_ADBUS_GPIO_MASK) >> N64_ADBUS_PIN_START);
}


void adbus_write_word(uint16_t data) {
    adbus_set_direction(true);
    uint32_t data_bits = ((uint32_t)data << N64_ADBUS_PIN_START);
    sio_hw->gpio_clr = N64_ADBUS_GPIO_MASK;
    sio_hw->gpio_set = data_bits;
    adbus_delay_nops(ADBUS_LATCH_DELAY_NOPS);
    sio_hw->gpio_clr = (1UL << N64_ADBUS_WR_PIN);
    adbus_delay_nops(ADBUS_WRITE_PULSE_NOPS);
    sio_hw->gpio_set = (1UL << N64_ADBUS_WR_PIN);
    adbus_delay_nops(ADBUS_LATCH_DELAY_NOPS);
    adbus_set_direction(false);
}


void adbus_assert_reset(bool active) {
    if (active) {
        gpio_put(N64_SYSTEM_RESET_PIN, 0);
        sleep_ms(20);
    } else {
        gpio_put(N64_SYSTEM_RESET_PIN, 1);
        sleep_ms(150);
    }
}
