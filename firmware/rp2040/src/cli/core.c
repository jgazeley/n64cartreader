// File: src/core.c
#include "cli/core.h"
#include "cli/command.h"
#include "cli/io.h"
#include "cli/menu.h"
#include "cli/menu_definitions.h"
#include "cli/plugins.h"

#include "n64/devices/gamepak.h"    /// NOTE: move to plugins somehow 

#include <ctype.h>
#include <stdio.h>

/*----------------------------------------------------------
 * Core state
 *----------------------------------------------------------*/
static bool s_ready         = false;
static bool s_cli_enabled   = false;
static bool s_was_connected = false;
static bool s_banner_shown  = false;
static bool s_skip_once      = false;
static const char * const *s_banner_lines = NULL;
static size_t             s_banner_count = 0;
static cli_mode_t         current_mode   = CLI_MODE_MENU;

/*----------------------------------------------------------
 * Default banner stored in read‐only flash, with compile‐time count
 *----------------------------------------------------------*/
const char * const default_banner[] = {
    "************** n64tool ***************",
    "    N64 Cartridge Reader / Writer     ",
    "======================================",
    "Version:   0.2.0-alpha",
    "Built:     " __DATE__,
};

void cli_set_banner(const char * const *lines, size_t count) {
    s_banner_lines = lines;
    s_banner_count = count;
}

// Print the banner once using our platform-agnostic I/O function
void cli_print_banner(void) {
    if (!s_banner_lines) return;

    for (size_t i = 0; i < s_banner_count; ++i) {
        cli_io_write_str(s_banner_lines[i]);
        cli_io_write_str("\n");
    }
    cli_io_write_str("\n");
}

/*----------------------------------------------------------
 * Initialization (called once at startup)
 *----------------------------------------------------------*/
bool cli_core_init(void)
{
    cli_io_init();
    cli_command_init();
    cli_plugins_initialize_all();

    application_menu_init();
    // Render the menu immediately on startup (before any connect)
    // menu_render();

    if (!s_banner_lines) {
        cli_set_banner(default_banner, DEFAULT_BANNER_LINES);
    }

    s_was_connected = false;
    s_banner_shown  = false;
    s_ready         = true;
    s_cli_enabled   = true;
    return true;
}

void cli_core_task(void)
{
    if (!s_ready) return;

    cli_io_poll();
    bool now_connected = cli_io_is_connected();

    if (now_connected && !s_was_connected) {
        // Print banner once
        if (!s_banner_shown) {
            cli_print_banner();
            s_banner_shown = true;
        }
        // Re-init and render menu prompt on every new connection:
        application_menu_init();
        menu_render();
    }
    s_was_connected = now_connected;
    if (!now_connected) return;

    /* disabled-CLI state: wait for any key to re-enable */
    if (!s_cli_enabled) {
        if (cli_io_read_char_nonblocking() >= 0) {
            s_cli_enabled = true;
            // show menu again when re-enabled
            menu_render();
        }
        return;
    }

    /* normal menu mode: read & dispatch a single printable char */
    if (current_mode == CLI_MODE_MENU) {
        int i = cli_io_read_char_nonblocking();
        if (i < 0 || !isprint(i)) return; // No valid input, do nothing.

        // If the check passed, proceed with handling the input as normal.
        char    raw = (char)i;
        char    cmd = (char)toupper((unsigned char)raw);

        /* echo and newline in one go */
        cli_io_write_str((char[]){ raw, '\n', '\0' });

        /* dispatch; menu_input returns false on invalid key */
        if (!menu_input(cmd)) {
            cli_io_write_str("Unknown option: '");
            cli_io_write_char(raw);
            cli_io_write_str("'\n");
            menu_render();
        }
    }
}