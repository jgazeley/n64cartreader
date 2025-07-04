#ifndef CLI_PLUGIN_TEST_H
#define CLI_PLUGIN_TEST_H

#include "cli/menu.h"

/**
 * @brief Registers the test plugin's commands with the CLI core.
 */
void plugin_test_register(void);

/**
 * @brief The test plugin's submenu frame.
 * Use &test_menu_frame in your root_items[].
 */
extern const menu_frame_t test_menu_frame;

#endif // CLI_PLUGIN_TEST_H
