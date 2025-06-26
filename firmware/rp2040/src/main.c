// File: src/main.c

#include <stdio.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "tusb.h"

#include "utils/transport.h"
#include "utils/packet.h"

#include "n64/bus/adbus.h"
#include "n64/devices/gamepak.h"

#ifdef ENABLE_CLI
#  include "cli/core.h"
#endif

// Forward declaration
static bool stream_gamepak(uint32_t base, size_t total);

int main(void) {
    stdio_init_all();
    tusb_init();
    sleep_ms(300);

    transport_init();
    adbus_init();

#ifdef ENABLE_CLI
    // Initialize CLI
    cli_core_init();
    printf("\nPico-Pak CLI Ready.\n");
#else
    // Streamer mode
    printf("\nPico-Pak Streamer Ready. Send 'G' to start.\n");
#endif

    while (true) {
        tud_task();

#ifdef ENABLE_CLI
        // Pump CLI
        cli_core_task();
#else
        // Wait for handshake 'G'
        if (tud_cdc_connected()) {
            uint8_t cmd;
            if (tud_cdc_read(&cmd, 1) == 1 && cmd == 'G') {
                // Dump first 8 MiB of the ROM
                stream_gamepak(N64_GAMEPAK_ROM_BASE, 8 PACKET_MB);
                tud_cdc_write_flush();
            }
        }
#endif
    }

    // unreachable
    return 0;
}

//------------------------------------------------------------------------------
// Read and stream `total` bytes from GamePak, 2 bytes per adbus_read_word()
static bool stream_gamepak(uint32_t base, size_t total) {
    static uint8_t buf[PACKET_WRITE_CHUNK_BYTES];
    size_t sent = 0;

    while (sent < total) {
        // ensure an even chunk size
        size_t chunk = PACKET_WRITE_CHUNK_BYTES & ~1u;
        if (sent + chunk > total) chunk = (total - sent) & ~1u;

        // fill buffer from ROM
        for (size_t i = 0; i < chunk; i += 2) {
            adbus_latch_address(base + sent + i);
            uint16_t w = adbus_read_word();
            buf[i]   = (uint8_t)(w >> 8);
            buf[i+1] = (uint8_t)(w & 0xFF);
        }

        // stream out
        if (!packet_stream(buf, chunk)) return false;
        sent += chunk;
    }
    return true;
}
