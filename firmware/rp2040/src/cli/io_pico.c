// ─────────────────────────────────────────────────────────────────────────────
// File: src/cli/io_pico.c
// Description: CLI I/O implementation for RP2040 Pico using TinyUSB (tud_cdc).
// ─────────────────────────────────────────────────────────────────────────────

#include "cli/io.h"
#include "pico/stdlib.h"
#include "tusb.h"

/**
 * cli_io_is_connected()
 *  - Returns true if the USB CDC interface is connected to a host.
 */
bool cli_io_is_connected(void) {
    return tud_cdc_connected();
}

/**
 * cli_io_sleep_ms()
 *  - Delay for a specified number of milliseconds.
 * @param ms  Number of milliseconds to sleep.
 */
void cli_io_sleep_ms(int ms) {
    sleep_ms(ms);
}

/**
 * cli_io_init()
 *  - Initialize all standard IO (UART, USB) for Pico.
 */
void cli_io_init(void) {
    // stdio_init_all();    <<<---///////////??????????????????????????????/////////////////////////??????????????
}

/**
 * cli_io_poll()
 *  - Poll the TinyUSB device stack to handle USB events.
 */
void cli_io_poll(void) {
    tud_task();
}

/**
 * cli_io_write_char()
 *  - Output a single character to the CLI.
 * @param c  Character to write.
 */
void cli_io_write_char(char c) {
    putchar(c);
}

/**
 * cli_io_write_str()
 *  - Output a null-terminated string to the CLI.
 * @param s  String to write.
 */
void cli_io_write_str(const char *s) {
    printf("%s", s);
}

/**
 * cli_io_read_char_nonblocking()
 *  - Read a character without blocking.
 *  - Returns -1 if no character is available.
 * @return Character read or -1 on timeout.
 */
int cli_io_read_char_nonblocking(void) {
    int c = getchar_timeout_us(0);
    return (c == PICO_ERROR_TIMEOUT) ? -1 : c;
}
