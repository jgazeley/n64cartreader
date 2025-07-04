// ─────────────────────────────────────────────────────────────────────────────
// File: src/cli/plugins.c
// Description: Plugin registration infrastructure for CLI commands and menus.
// ─────────────────────────────────────────────────────────────────────────────

#include "cli/plugins.h"
#include <stddef.h>

#define MAX_PLUGINS 10    ///< Maximum number of plugins for commands or menus

//------------------------------------------------------------------------------
// Static Storage for Plugin Init Callbacks
//------------------------------------------------------------------------------
/// Array of registered command‐init functions
static cli_plugin_init_fn s_command_init_funcs[MAX_PLUGINS];
/// Array of registered menu‐init functions
static cli_plugin_init_fn s_menu_init_funcs[MAX_PLUGINS];
/// Number of registered command plugins
static size_t s_num_command_plugins = 0;
/// Number of registered menu plugins
static size_t s_num_menu_plugins    = 0;

//------------------------------------------------------------------------------
// Public Plugin API
//------------------------------------------------------------------------------

/**
 * cli_register_command_init()
 *  - Register a plugin’s command‐initialization function.
 *  - Silently ignores NULL or if limit reached.
 */
void cli_register_command_init(cli_plugin_init_fn func) {
    if (func && s_num_command_plugins < MAX_PLUGINS) {
        s_command_init_funcs[s_num_command_plugins++] = func;
    }
}

/**
 * cli_register_menu_init()
 *  - Register a plugin’s menu‐initialization function.
 *  - Silently ignores NULL or if limit reached.
 */
void cli_register_menu_init(cli_plugin_init_fn func) {
    if (func && s_num_menu_plugins < MAX_PLUGINS) {
        s_menu_init_funcs[s_num_menu_plugins++] = func;
    }
}

/**
 * cli_plugins_initialize_all()
 *  - Invoke all registered command plugins, then all menu plugins.
 *  - Called once during CLI core initialization.
 */
void cli_plugins_initialize_all(void) {
    // Initialize all command plugins
    for (size_t i = 0; i < s_num_command_plugins; i++) {
        s_command_init_funcs[i]();
    }

    // Initialize all menu plugins
    for (size_t i = 0; i < s_num_menu_plugins; i++) {
        s_menu_init_funcs[i]();
    }
}
