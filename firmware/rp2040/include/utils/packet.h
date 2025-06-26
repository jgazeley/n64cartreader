// File: include/utils/packet.h
#ifndef UTILS_PACKET_H
#define UTILS_PACKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* --- Module-local size helpers (number + unit) --- */
#define PACKET_KB   * 1024u
#define PACKET_MB   * 1024u * 1024u

/* --- Configurable limits --- */
/** Maximum payload size for packet framing (bytes) */
#ifndef PACKET_MAX_PAYLOAD
#define PACKET_MAX_PAYLOAD   (1024u)
#endif

/** Default chunk size per packet_stream() write burst (bytes) */
#ifndef PACKET_WRITE_CHUNK_BYTES
// move off stack if >4KiB
#define PACKET_WRITE_CHUNK_BYTES (8 PACKET_KB)
#endif

/* --- Framed packet API --- */
/** One framed packet ([0xAA,55][len_hi,len_lo][payload][crc_hi,crc_lo]) */
bool packet_send(const uint8_t *payload, uint16_t len);
int  packet_receive(uint8_t *out_buf, size_t max_len);

/* --- Raw-stream API --- */
/** Stream raw bytes (no framing) in write bursts of PACKET_WRITE_CHUNK_BYTES */
bool packet_stream(const uint8_t *data, size_t total);

/** Stream a 0–255 repeating test pattern of `total` bytes */
bool packet_stream_pattern(size_t total);

#endif // UTILS_PACKET_H