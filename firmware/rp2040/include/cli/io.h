#ifndef CLI_IO_H
#define CLI_IO_H

#include <stdbool.h>

/**
 * @file io.h
 * @brief Platform Abstraction Layer for j-shell I/O.
 *
 * The application/platform MUST provide implementations for these functions.
 */

/**
 * @brief Perform platform-specific initialization for I/O.
 */
void cli_io_init(void);

/**
 * @brief Perform periodic tasks required by the platform (e.g., USB polling).
 */
void cli_io_poll(void);

/**
 * @brief Check if the I/O connection is active.
 * @return true if connected, false otherwise.
 */
bool cli_io_is_connected(void);

/**
 * @brief Read a single character in a non-blocking manner.
 * @return The character read (0-255), or -1 if no character is available.
 */
int cli_io_read_char_nonblocking(void);

/**
 * @brief Write a single character.
 */
void cli_io_write_char(char c);

/**
 * @brief Write a null-terminated string.
 */
void cli_io_write_str(const char *s);

/**
 * @brief Sleep for a specified number of milliseconds.
 */
void cli_io_sleep_ms(int ms);

#endif // CLI_IO_H