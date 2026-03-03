// File: include/cli/menu.h
#ifndef CLI_MENU_H
#define CLI_MENU_H

#include <stddef.h>
#include <stdbool.h>

#define MAX_MENU_DEPTH 5
#define MAGIC_KEY  0xA5

// --- Data Structures ---

typedef struct menu_item_t {
    char                  key;
    const char           *desc;
    const char           *cmd_name;
    const struct menu_frame_t *submenu;
} menu_item_t;

struct menu_item_t;

typedef struct menu_frame_t {
    const char      *title;
    const menu_item_t *items;
    size_t           count;
} menu_frame_t;


// --- Core Menu Engine API ---

/**
 * @brief Initializes the menu system with a starting frame.
 * This sets the root of the menu stack.
 * @param root_frame A pointer to the top-level menu frame.
 */
void menu_init(const menu_frame_t *root_frame);

/**
 * @brief Renders the menu frame currently at the top of the stack.
 */
void menu_render(void);

/**
 * @brief Processes a single character of user input for the current menu.
 * @param ch The character input by the user.
 * @return true if the character was a valid menu option, false otherwise.
 */
bool menu_input(char ch);

#endif // CLI_MENU_H
