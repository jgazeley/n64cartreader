// File: include/cli/menu_definitions.h
#ifndef CLI_MENU_DEFINITIONS_H
#define CLI_MENU_DEFINITIONS_H

#include "cli/menu.h"

/**
 * @brief The top‐level root menu frame, built from all plugins.
 *
 * Defined in src/cli/menu_definitions.c.
 */
extern const menu_frame_t root_menu_frame;

/**
 * @brief Initialize the menu system to the application root frame.
 *
 * Calls menu_init(&root_menu_frame).
 */
void application_menu_init(void);

#endif // CLI_MENU_DEFINITIONS_H
