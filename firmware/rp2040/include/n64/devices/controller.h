/**
 * @file controller.h
 * @brief N64 controller port interface and state definitions.
 *
 * Defines the controller state structure, button bitmasks, and
 * functions for initializing, polling controllers, and Controller Pak.
 */
#ifndef N64_CONTROLLER_H
#define N64_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//------------------------------------------------------------------------------
// Controller State Structure
//------------------------------------------------------------------------------

/**
 * @brief Represents the current state of an N64 controller.
 */
typedef struct {
    uint16_t buttons; /**< Bitmask of pressed buttons */
    int8_t   stick_x; /**< Analog stick X-axis (-128 to +127) */
    int8_t   stick_y; /**< Analog stick Y-axis (-128 to +127) */
} n64_controller_state_t;

//------------------------------------------------------------------------------
// Button Bitmask Definitions
//------------------------------------------------------------------------------

#define N64_BUTTON_A         (1u << 15) /**< A button */
#define N64_BUTTON_B         (1u << 14) /**< B button */
#define N64_BUTTON_Z         (1u << 13) /**< Z trigger */
#define N64_BUTTON_START     (1u << 12) /**< Start */
#define N64_BUTTON_D_UP      (1u << 11) /**< D-pad Up */
#define N64_BUTTON_D_DOWN    (1u << 10) /**< D-pad Down */
#define N64_BUTTON_D_LEFT    (1u <<  9) /**< D-pad Left */
#define N64_BUTTON_D_RIGHT   (1u <<  8) /**< D-pad Right */
#define N64_BUTTON_L         (1u <<  5) /**< L shoulder */
#define N64_BUTTON_R         (1u <<  4) /**< R shoulder */
#define N64_BUTTON_C_UP      (1u <<  3) /**< C-button Up */
#define N64_BUTTON_C_DOWN    (1u <<  2) /**< C-button Down */
#define N64_BUTTON_C_LEFT    (1u <<  1) /**< C-button Left */
#define N64_BUTTON_C_RIGHT   (1u <<  0) /**< C-button Right */

//------------------------------------------------------------------------------
// Initialization and Polling
//------------------------------------------------------------------------------

/**
 * @brief Initialize the controller interface (PIO/SIO setup).
 *
 * Must be called before any controller operations.
 *
 * @return true on success, false on error
 */
bool controller_init(void);

/**
 * @brief Read the current state of the controller.
 *
 * @param state Pointer to state struct to populate
 * @return true if read succeeds, false on failure or timeout
 */
bool controller_read(n64_controller_state_t *state);

//------------------------------------------------------------------------------
// Controller Pak (Memory Pak) Operations
//------------------------------------------------------------------------------

/**
 * @brief Initialize the Controller Pak (memory) interface.
 *
 * Detects and prepares for read/write operations.
 *
 * @return true if a pak is detected and initialized
 */
bool controller_pak_init(void);

/**
 * @brief Read data from the Controller Pak.
 *
 * @param buffer Destination buffer for read data
 * @param len    Number of bytes to read
 * @return true on successful read
 */
bool controller_pak_read(uint8_t *buffer, size_t len);

/**
 * @brief Write data to the Controller Pak.
 *
 * @param buffer Source buffer containing data to write
 * @param len    Number of bytes to write
 * @return true on successful write
 */
bool controller_pak_write(const uint8_t *buffer, size_t len);

#endif // N64_CONTROLLER_H
