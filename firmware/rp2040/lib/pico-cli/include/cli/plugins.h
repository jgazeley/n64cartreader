// File: include/cli/plugins.h (Final Version)

#ifndef CLI_PLUGINS_H
#define CLI_PLUGINS_H

// A function pointer for a plugin's initialization routine.
typedef void (*cli_plugin_init_fn)(void);

/**
 * @brief Registers a function that initializes a plugin's commands.
 */
void cli_register_command_init(cli_plugin_init_fn func);

/**
 * @brief Registers a function that initializes a plugin's menus.
 */
void cli_register_menu_init(cli_plugin_init_fn func);

/**
 * @brief Called by the CLI core to run all registered init functions.
 * @note For internal use by cli_core_init() only.
 */
void cli_plugins_initialize_all(void);

#endif // CLI_PLUGINS_H