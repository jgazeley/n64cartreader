// File: include/utils/transport.h
#ifndef UTILS_TRANSPORT_H
#define UTILS_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Low‐level byte‐stream transport interface.
 *
 * Implementations should send `len` bytes from `data`, returning false
 * on unrecoverable error.
 */
typedef struct {
    bool  (*write_bytes)(const uint8_t *data, size_t len);
    void  (*flush)(void);
    bool  (*read_byte)(uint8_t *out);   ///< New: nonblocking single-byte read
} transport_t;

/**
 * @brief Register the transport backend for packet_send().
 *
 * Must be called before using utils/packet_send().
 */
void transport_register(const transport_t *t);

/**
 * @brief Convenience init for your platform’s default transport.
 *        Implemented in transport.c.
 */
void transport_init(void);

/**
 * @brief Retrieve the currently registered transport.
 *
 * For internal use by packet.c.
 */
const transport_t *transport_get(void);

#endif // UTILS_TRANSPORT_H
