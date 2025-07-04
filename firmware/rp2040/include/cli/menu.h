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

// old menu structure
// typedef struct menu_item_t {
//     char                 key;        // The character input to select this item
//     const char          *desc;      // Description displayed to the user
//     const char          *cmd_name;     // Function to call if this is a command
//     const menu_item_t   *submenu; // Pointer to a submenu if this is a menu entry
//     size_t               submenu_size; // Number of items in the submenu
//     const char          *submenu_title; // NEW: Title for the submenu, if 'submenu' is not NULL
// } menu_item_t;


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











// /*
//  * include/cli/menu.h
//  * Menu data structures
//  */
// #ifndef CLI_MENU_H
// #define CLI_MENU_H

// #include <stddef.h>
// #include <stdbool.h> // For boolean types

// // Forward declare for submenu pointers
// struct menu_item_t;
// typedef struct menu_item_t menu_item_t;

// // Function pointer type for CLI actions
// typedef void (*cli_action_t)(void);

// // Structure representing a single item in a menu
// typedef struct menu_item_t {
//     char            key;        // The character input to select this item
//     const char      *desc;      // Description displayed to the user
//     cli_action_t    action;     // Function to call if this is a command
//     const menu_item_t *submenu; // Pointer to a submenu if this is a menu entry
//     size_t          submenu_size; // Number of items in the submenu
//     const char      *submenu_title; // NEW: Title for the submenu, if 'submenu' is not NULL
// } menu_item_t;

// extern const menu_item_t n64_menu_items[];
// extern const size_t      n64_menu_size;
// extern const char       *n64_menu_title;

// // ----------------------------------------
// // Forward declarations for command action functions
// // ----------------------------------------
// extern void cmd_debug_show_info(void);
// // extern void cmd_debug_show_header(void);
// // extern void cmd_debug_show_sram(void);
// // extern void cmd_debug_show_eeprom(void);
// // extern void cmd_debug_dump_sram(void);
// // extern void cmd_debug_dump_eeprom(void);
// // extern void cmd_debug_write32_sram(void);
// // extern void cmd_debug_write32_eeprom(void);
// // extern void cmd_debug_test_controller(void);
// // extern void cmd_n64_gameshark(void);
// // extern void cmd_n64_repro(void);
// // extern void cmd_n64_read_mpk(void);
// // extern void cmd_n64_write_mpk(void);
// // extern void cmd_n64_dump_rom(void);
// // extern void cmd_n64_read_save(void);
// // extern void cmd_n64_write_save(void);
// // extern void cmd_debug_verify_crc(void);   // doesn't work

// // Structure to hold the state of a menu on the navigation stack
// typedef struct {
//     const menu_item_t *items;
//     size_t count;
//     const char *title;
// } menu_frame_t;

// // Define the maximum depth of nested menus to prevent stack overflow
// #define MAX_MENU_DEPTH 5

// // --- Public API for Menu Management ---

// /**
//  * @brief Initializes the menu system with a given set of menu items.
//  * @param items Pointer to the array of menu_item_t for the initial menu.
//  * @param count The number of items in the initial menu.
//  * @param title The title of the initial menu.
//  */
// void menu_init(const menu_item_t *items, size_t count, const char *title);

// /**
//  * @brief Renders the current menu to the console.
//  * Displays the menu title, all available options, and a prompt.
//  */
// void menu_render(void);

// /**
//  * @brief Processes a single character input for the current menu.
//  * Navigates menus or executes actions based on the input character.
//  * @param ch The character input by the user.
//  */
// void menu_input(char ch);

// // /**
// //  * @brief Initializes and displays the main N64 menu.
// //  * This is the default starting point for the CLI.
// //  */
// // void menu_init_n64(void);

// // /**
// //  * @brief Initializes and displays the debug menu.
// //  * Used for specific debugging scenarios.
// //  */
// // void menu_init_debug(void);

// // #endif // CLI_MENU_H
