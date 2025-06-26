/**
 * @file    cli/core.h
 * @brief   Public API for the main CLI core engine.
 */
#ifndef CLI_CORE_H
#define CLI_CORE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef ENABLE_CLI

/**
 * Defines the possible CLI interaction modes.
 */
typedef enum {
    CLI_MODE_MENU,    /**< Character-based menu navigation */
    CLI_MODE_PARSER   /**< Line-based command parsing (future) */
} cli_mode_t;

/**
 * Initialize the CLI system.
 * Must be called once at startup, after stdio/USB is up.
 *
 * @return true on success, false on failure.
 */
bool cli_core_init(void);

/**
 * The main task function for the CLI.
 * Call continuously in the main loop to poll for input.
 */
void cli_core_task(void);

/**
 * Check whether the CLI backend is ready to accept input.
 *
 * @return true if ready, false otherwise.
 */
bool cli_core_is_ready(void);

/**
 * Switch the CLI's interaction mode.
 *
 * @param new_mode The mode to switch to.
 */
void cli_core_set_mode(cli_mode_t new_mode);

/**
 * Install a custom banner to print at the top of each menu.
 *
 * @param lines Array of NUL-terminated strings.
 * @param count Number of lines in the array.
 */
void cli_set_banner(const char * const *lines, size_t count);

/**
 * @brief Disable the CLI loop (soft-exit).
 *
 * After calling this, cli_core_task() will early-out until re-enabled.
 */
void cli_core_disable(void);

/**
 * @brief Re-enable the CLI loop.
 *
 * Prints the banner & root menu immediately on re-enable.
 */
void cli_core_enable(void);
/**
 * Returns true if the CLI loop is currently enabled.
 * Only valid when ENABLE_CLI is defined.
 */
bool cli_core_is_enabled(void);

#else  /* !ENABLE_CLI: provide no-op stubs */

typedef enum { CLI_MODE_MENU, CLI_MODE_PARSER } cli_mode_t;

static inline bool cli_core_init(void)               { return true;  }
static inline void cli_core_task(void)               {              }
static inline bool cli_core_is_ready(void)           { return false; }
static inline void cli_core_set_mode(cli_mode_t m)   { (void)m;     }
static inline void cli_set_banner(const char * const *l, size_t c) { (void)l; (void)c; }
static inline void cli_core_disable(void)            {              }
static inline void cli_core_enable(void)             {              }
static inline bool cli_core_is_enabled(void)         { return false; }

#endif /* ENABLE_CLI */

#endif /* CLI_CORE_H */