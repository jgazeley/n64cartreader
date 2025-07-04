// // include/cli/plugins.h
// #ifndef CLI_PLUGINS_H
// #define CLI_PLUGINS_H

// #include <stddef.h>

// // A plugin is just a function that registers commands or menus.
// typedef void (*cli_plugin_fn)(void);

// // These two arrays must be defined exactly once (in plugins.c)
// extern const cli_plugin_fn cli_command_plugins[];
// extern const size_t        cli_num_command_plugins;

// extern const cli_plugin_fn cli_menu_plugins[];
// extern const size_t        cli_num_menu_plugins;

// #endif // CLI_PLUGINS_H











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