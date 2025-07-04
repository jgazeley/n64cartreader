#ifndef CLI_PLUGIN_N64_H
#define CLI_PLUGIN_N64_H

#include "cli/menu.h"

#include "n64/bus/adbus.h"
#include "n64/bus/joybus.h"
#include "n64/devices/gamepak.h"


/**
 * @brief Registers the N64 plugin's commands with the CLI core.
 */
void plugin_n64_register(void);

/**
 * @brief The N64 plugin's submenu frame.
 * Use &n64_menu_frame in your root_items[].
 */
extern const menu_frame_t n64_menu_frame;

#endif // CLI_PLUGIN_N64_H
