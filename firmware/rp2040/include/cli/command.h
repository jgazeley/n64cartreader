/**
 * @file    cli/command.h
 * @brief   Public API for registering, looking up, and invoking CLI commands.
 *
 * This header defines:
 *  - A flexible registry so each subsystem can add its own commands
 *  - A uniform signature for command handlers (argc/argv style)
 *  - Helpers to look up commands by name and to dispatch them
 */

#ifndef CLI_COMMAND_H
#define CLI_COMMAND_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Parsed argument list for a command.
 */
typedef struct {
    size_t      argc;    /**< Count of arguments (excluding the command name) */
    const char *argv[];  /**< NULL-terminated array of C-string pointers */
} cli_args_t;

/**
 * @brief Prototype for any CLI command handler.
 *
 * @param args  Parsed arguments (may be NULL if none).
 * @return      true on success, false on error.
 */
typedef bool (*cli_command_fn_t)(const cli_args_t *args);

/**
 * @brief One entry in the CLI command registry.
 */
typedef struct {
    const char         *name;   /**< Command string, e.g. "readrom" */
    cli_command_fn_t    fn;     /**< Handler function */
    const char         *help;   /**< Short help/usage text */
} cli_command_t;

/**
 * @brief Initialize the command subsystem.
 *
 * Registers built-in commands (help, exit, settings, etc.).
 * Must be called before any other cli_command_* functions.
 */
void cli_command_init(void);

/**
 * @brief Register an array of commands.
 *
 * Subsystems (GamePak, Controller, Debug, etc.) call this at startup.
 *
 * @param cmds   Pointer to an array of cli_command_t.
 * @param count  Number of entries in that array.
 */
void cli_command_register(const cli_command_t *cmds, size_t count);

/**
 * @brief Look up a command by name.
 *
 * @param name       The command name (case-sensitive).
 * @param out_cmd    If found, receives pointer to the matching descriptor.
 * @return           true if the command exists, false otherwise.
 */
bool cli_command_lookup(const char *name, const cli_command_t **out_cmd);

/**
 * @brief Invoke a command by its descriptor.
 *
 * @param cmd   Pointer to the cli_command_t returned by cli_command_lookup().
 * @param args  Parsed arguments (or NULL if none).
 * @return      The boolean return of the handler.
 */
bool cli_command_execute(const cli_command_t *cmd, const cli_args_t *args);

/**
 * @brief Invoke by name in one step (lookup + execute).
 *
 * @param name  Command name.
 * @param args  Parsed arguments (or NULL if none).
 * @return      true on success, false on error/unknown command.
 */
bool cli_command_run(const char *name, const cli_args_t *args);

/**
 * @brief Get a list of all registered commands.
 *
 * @param out_cmds   Receives pointer to internal array (read-only).
 * @param out_count  Receives number of entries.
 */
void cli_command_list(const cli_command_t **out_cmds, size_t *out_count);

// void exit_cmd_register(void);

#endif // CLI_COMMAND_H
