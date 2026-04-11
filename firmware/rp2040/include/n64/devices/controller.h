/**
 * @file controller.h
 * @brief N64 controller and Memory Pak interface.
 */
#ifndef N64_CONTROLLER_H
#define N64_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t device_id;
    uint8_t status;
} n64_controller_info_t;

typedef struct {
    uint16_t buttons;
    int8_t stick_x;
    int8_t stick_y;
} n64_controller_state_t;

#define N64_MEMPAK_SIZE 32768u
#define N64_MEMPAK_BLOCK_SIZE 32u

// Button bit layout follows standard N64 2-byte poll response.
#define N64_BUTTON_A         (1u << 15)
#define N64_BUTTON_B         (1u << 14)
#define N64_BUTTON_Z         (1u << 13)
#define N64_BUTTON_START     (1u << 12)
#define N64_BUTTON_D_UP      (1u << 11)
#define N64_BUTTON_D_DOWN    (1u << 10)
#define N64_BUTTON_D_LEFT    (1u <<  9)
#define N64_BUTTON_D_RIGHT   (1u <<  8)
#define N64_BUTTON_L         (1u <<  5)
#define N64_BUTTON_R         (1u <<  4)
#define N64_BUTTON_C_UP      (1u <<  3)
#define N64_BUTTON_C_DOWN    (1u <<  2)
#define N64_BUTTON_C_LEFT    (1u <<  1)
#define N64_BUTTON_C_RIGHT   (1u <<  0)

bool controller_init(void);
bool controller_probe(n64_controller_info_t *info);
bool controller_read(n64_controller_state_t *state, bool rumble);
bool controller_is_connected(void);

bool controller_mempak_present(void);
bool controller_mempak_read_block(uint16_t address, uint8_t out[32]);
bool controller_mempak_write_block(uint16_t address, const uint8_t data[32]);
bool controller_mempak_read_all(uint8_t *out, size_t len);
bool controller_mempak_write_all(const uint8_t *data, size_t len);

#endif // N64_CONTROLLER_H
