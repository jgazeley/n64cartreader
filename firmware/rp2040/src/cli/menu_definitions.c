// ─────────────────────────────────────────────────────────────────────────────
// File: src/cli/menu_definitions.c
// Description: Root menu definition for the CLI, linking core commands and plugins.
// ─────────────────────────────────────────────────────────────────────────────

#include "cli/menu.h"
#include "cli/core.h"
#include "cli/plugin_n64.h"

const char * const default_banner[] = {
    "**************************************",
    "    N64 Cartridge Reader / Writer     ",
    "======================================",
    "Version:   0.3.0-alpha",
    "Built:     " __DATE__,
};
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

//------------------------------------------------------------------------------
// Root Menu Items
//------------------------------------------------------------------------------
/// key   desc            cmd_name    submenu
static const menu_item_t root_items[] = {
   // { 'T', "Test Commands", NULL,             &test_menu_frame  },   ///< Plugin: fill/dump/write tests
    { 'N', "N64 Options",   NULL,             &n64_menu_frame   },      ///< Plugin: N64 header/info
    // { 'V', "Version",       "version",        NULL              },   ///< Core: show firmware version
    // { 'U', "Uptime",        "uptime",         NULL              },   ///< Core: show ms since reset
    // { 'M', "Memory",        "mem",            NULL              },   ///< Core: report free/used RAM
    // { 'P', "Ping",          "ping",           NULL              },   ///< Core: respond with “pong”
    // { 'B', "Banner",        "banner",         NULL              },   ///< Core: reprint the banner
    // { 'H', "Help",          "help",           NULL              },      ///< Core: show help list
    { 'C', "Command Prompt",  "shell",          NULL },
    { 'X', "Exit CLI",      "exit",           NULL              },      ///< Core: reboot/exit
};

//------------------------------------------------------------------------------
// Root Menu Frame
//------------------------------------------------------------------------------
const menu_frame_t root_menu_frame = {
    .title = "Main Menu",
    .items = root_items,
    .count = sizeof(root_items) / sizeof(*root_items),
};

//------------------------------------------------------------------------------
// Application Menu Initialization
//------------------------------------------------------------------------------
/// Called by cli_core_init() to install the root menu
void application_menu_init(void) {
    // App identity lives here now (NOT in core)
    cli_set_banner(default_banner, ARRAY_LEN(default_banner));
    cli_core_set_prompt("pico-pak> ");

    menu_init(&root_menu_frame);
}
