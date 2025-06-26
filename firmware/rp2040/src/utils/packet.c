// File: src/utils/packet.c

#include "utils/packet.h"
#include "utils/transport.h"
#include "utils/crc.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

// move the large buffer out of the stack, into BSS:
static uint8_t chunk_buf[PACKET_WRITE_CHUNK_BYTES];

/**
 * @brief Send one framed packet over the registered transport.
 *
 * Framing: [0xAA,0x55][len_hi,len_lo][payload...][crc_hi,crc_lo].
 */
bool packet_send(const uint8_t *payload, uint16_t len) {
    const transport_t *t = transport_get();
    if (!t || !t->write_bytes) return false;

    uint8_t header[4] = {
        0xAA, 0x55,
        (uint8_t)(len >> 8),
        (uint8_t)(len & 0xFF)
    };
    uint16_t crc = crc16_ibm(payload, len);
    uint8_t footer[2] = {
        (uint8_t)(crc >> 8),
        (uint8_t)(crc & 0xFF)
    };

    if (!t->write_bytes(header, sizeof header))   return false;
    if (!t->write_bytes(payload, len))            return false;
    if (!t->write_bytes(footer, sizeof footer))   return false;
    t->flush();
    return true;
}


/**
 * @brief Receive one framed packet over the registered transport.
 *
 * Returns:
 *   >0 : number of payload bytes (written into out_buf)
 *    0 : no complete packet yet
 *   -1 : CRC error
 */
int packet_receive(uint8_t *out_buf, size_t max_len) {
    static enum { SYNC1, SYNC2, LEN1, LEN2, DATA, CRC1, CRC2 } state;
    static uint16_t len, idx, crc_recv, crc_calc;
    static uint8_t buf[PACKET_MAX_PAYLOAD];

    const transport_t *t = transport_get();
    if (!t || !t->read_byte) return -1;

    uint8_t b;
    while (t->read_byte(&b)) {
        switch (state) {
            case SYNC1:
                if (b == 0xAA) state = SYNC2;
                break;
            case SYNC2:
                if (b == 0x55) state = LEN1;
                else state = SYNC1;
                break;
            case LEN1:
                len = ((uint16_t)b) << 8;
                state = LEN2;
                break;
            case LEN2:
                len |= b;
                if (len > sizeof buf) {
                    state = SYNC1;
                    break;
                }
                idx      = 0;
                crc_calc = 0;
                state    = (len ? DATA : CRC1);
                break;
            case DATA:
                buf[idx++] = b;
                crc_calc   = crc16_update(crc_calc, &b, 1);
                if (idx == len) state = CRC1;
                break;

            case CRC1:
                crc_recv = ((uint16_t)b) << 8;
                state    = CRC2;
                break;
            case CRC2:
                crc_recv |= b;
                if (crc_recv == crc_calc && len <= max_len) {
                    memcpy(out_buf, buf, len);
                    state = SYNC1;
                    return (int)len;
                } else {
                    // CRC mismatch or overflow
                    state = SYNC1;
                    return -1;
                }
        }
    }
    return 0;  // still waiting for the rest of the packet
}


/**
 * @brief Stream raw bytes (no framing) in chunked writes.
 *
 * Useful for maximum-throughput transfers of large buffers.
 */
bool packet_stream(const uint8_t *data, size_t total) {
    const transport_t *t = transport_get();
    if (!t) return false;

    size_t sent = 0;
    while (sent < total) {
        size_t n = total - sent;
        if (n > PACKET_WRITE_CHUNK_BYTES) n = PACKET_WRITE_CHUNK_BYTES;
        if (!t->write_bytes(&data[sent], n)) return false;
        sent += n;
    }
    t->flush();
    return true;
}

bool packet_stream_pattern(size_t total) {
    size_t sent = 0;
    while (sent < total) {
        size_t n = total - sent;
        if (n > PACKET_WRITE_CHUNK_BYTES) n = PACKET_WRITE_CHUNK_BYTES;
        for (size_t i = 0; i < n; i++) {
            chunk_buf[i] = (uint8_t)((sent + i) & 0xFF);
        }
        if (!packet_stream(chunk_buf, n)) return false;
        sent += n;
    }
    return true;
}