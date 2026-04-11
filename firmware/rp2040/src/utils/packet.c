// File: src/utils/packet.c

#include "utils/packet.h"
#include "utils/transport.h"
#include "utils/crc.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#define ARQ_ACK 0x06
#define ARQ_NAK 0x15
#define ARQ_MAX_RETRIES 5
#define ARQ_TIMEOUT_MS 500

// move the large buffer out of the stack, into BSS:
static uint8_t chunk_buf[PACKET_WRITE_CHUNK_BYTES];

/**
 * @warning DEPRECATED.
 * This uses a 4-byte header and NO checksum.
 * Incompatible with packet_receive_reliable().
 */
bool packet_send(const uint8_t *payload, uint16_t len) {
    const transport_t *t = transport_get();
    if (!t || !t->write_bytes) return false;

    uint8_t header[4] = {
        0xAA, 0x55,
        (uint8_t)(len >> 8),
        (uint8_t)(len & 0xFF)
    };
    uint16_t crc = crc16_update(0, payload, len);
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
 * @brief Sends a packet using the 5-byte Reliable Header format.
 * * Format: SYNC_H, SYNC_L, SEQ, LEN_H, LEN_L, [DATA], CRC_H, CRC_L
 * * Use this for all N64 Save/ROM data transfers.
 */
bool packet_send_reliable(const uint8_t *payload, uint16_t len, uint8_t seq) {
    const transport_t *t = transport_get();
    if (!t || !t->write_bytes || !t->read_with_timeout) return false;

    // New Header: [Sync1, Sync2, Seq, Len_Hi, Len_Lo]
    uint8_t header[5] = {
        0xAA, 0x55,
        seq,
        (uint8_t)(len >> 8),
        (uint8_t)(len & 0xFF)
    };

    uint16_t crc = crc16_update(0, payload, len);
    uint8_t footer[2] = {
        (uint8_t)(crc >> 8),
        (uint8_t)(crc & 0xFF)
    };

    int retries = 0;
    while (retries < ARQ_MAX_RETRIES) {
        // 1. Blast the data
        t->write_bytes(header, sizeof(header));
        t->write_bytes(payload, len);
        t->write_bytes(footer, sizeof(footer));
        t->flush();

        // 2. Wait for PC response
        uint8_t response;
        if (t->read_with_timeout(&response, ARQ_TIMEOUT_MS)) {
            if (response == ARQ_ACK) {
                return true; // PC got it perfectly!
            }
            // If it's a NAK or random garbage, we drop through and retry
        }

        // 3. Either it timed out or we got a NAK.
        retries++;
    }

    // Hit the retry limit. The link is officially dead.
    return false;
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


// Receiver that matches the 5-byte Reliable format
bool packet_receive_reliable(uint8_t *buffer, uint16_t expected_len, uint8_t expected_seq) {
    const transport_t *t = transport_get();
    if (!t || !t->read_with_timeout) return false;

    int retries = 0;
    while (retries < ARQ_MAX_RETRIES) {
        uint8_t b;

        // 1. Wait for Sync Header (0xAA 0x55)
        if (!t->read_with_timeout(&b, ARQ_TIMEOUT_MS)) { retries++; continue; }
        if (b != 0xAA) continue;
        if (!t->read_with_timeout(&b, ARQ_TIMEOUT_MS) || b != 0x55) continue;

        // 2. Read Sequence and Length
        uint8_t seq, len_hi, len_lo;
        if (!t->read_with_timeout(&seq, ARQ_TIMEOUT_MS)) continue;
        if (!t->read_with_timeout(&len_hi, ARQ_TIMEOUT_MS)) continue;
        if (!t->read_with_timeout(&len_lo, ARQ_TIMEOUT_MS)) continue;

        uint16_t payload_len = (uint16_t)(len_hi << 8) | len_lo;

        // If the PC sent the wrong size, reject it immediately
        if (payload_len != expected_len) {
            uint8_t nak = ARQ_NAK;
            t->write_bytes(&nak, 1);
            t->flush();
            continue;
        }

        // 3. Read Payload directly into the buffer
        bool payload_ok = true;
        for (uint16_t i = 0; i < payload_len; i++) {
            if (!t->read_with_timeout(&buffer[i], ARQ_TIMEOUT_MS)) {
                payload_ok = false;
                break;
            }
        }
        if (!payload_ok) continue;

        // 4. Read PC's CRC
        uint8_t crc_hi, crc_lo;
        if (!t->read_with_timeout(&crc_hi, ARQ_TIMEOUT_MS)) continue;
        if (!t->read_with_timeout(&crc_lo, ARQ_TIMEOUT_MS)) continue;
        uint16_t received_crc = (uint16_t)(crc_hi << 8) | crc_lo;

        // 5. Verify Math & Respond
        uint16_t calc_crc = crc16_update(0, buffer, payload_len);
        if (calc_crc == received_crc && seq == expected_seq) {
            uint8_t ack = ARQ_ACK;
            t->write_bytes(&ack, 1);
            t->flush();
            return true; // Success!
        } else {
            uint8_t nak = ARQ_NAK;
            t->write_bytes(&nak, 1);
            t->flush();
            retries++;
        }
    }
    return false; // Link dead or too many errors
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
    const transport_t *t = transport_get();
    if (!t || !t->write_bytes) return false;

    size_t sent = 0;
    while (sent < total) {
        size_t n = total - sent;
        if (n > PACKET_WRITE_CHUNK_BYTES) n = PACKET_WRITE_CHUNK_BYTES;

        for (size_t i = 0; i < n; i++) {
            chunk_buf[i] = (uint8_t)((sent + i) & 0xFF);
        }

        if (!t->write_bytes(chunk_buf, n)) return false;
        sent += n;
    }
    if (t->flush) t->flush();
    return true;
}