// File: src/cli/core.c
#include "cli/core.h"

#ifdef ENABLE_CLI

#include "cli/menu.h"
#include "cli/command.h"

#include "pico/stdlib.h"
#include "tusb.h"
#include <stdio.h>

static bool        s_ready          = false;
static bool        s_was_connected  = false;
static bool        s_banner_shown   = false;
static bool        s_cli_enabled    = false;
static const char * const *s_banner_lines = NULL;
static size_t             s_banner_count = 0;
static cli_mode_t         current_mode   = CLI_MODE_MENU;
// in src/cli/core.c, among your static globals add:
static bool s_skip_once = false;

// Print the banner once
static void cli_print_banner(void) {
    if (!s_banner_lines) return;
    for (size_t i = 0; i < s_banner_count; ++i) {
        printf("%s\n", s_banner_lines[i]);
    }
    printf("\n");
}

void cli_set_banner(const char * const *lines, size_t count) {
    s_banner_lines = lines;
    s_banner_count = count;
}

/**
 * @brief Initialize the CLI system.
 * @return true on success (CLI ready to accept input), false on failure.
 */
// bool cli_core_init(void) {
//     // --- No stdio_init_all()/tusb_init() here! those happen in main() ---

//     transport_init();          // register the CDC transport
//     cli_command_init();        // register built-in commands

//     // Default banner if none set
//     static const char *default_banner[] = {
//         "=== Project Console ===",
//         "Version: 1.0.0",
//         "Built:   " __DATE__,
//         "Board:   MCU???"
//     };
//     if (!s_banner_lines) {
//         s_banner_lines = default_banner;
//         s_banner_count = sizeof default_banner / sizeof *default_banner;
//     }

//     // Show banner + initial menu
//     cli_print_banner();
//     menu_init_root();

//     s_ready = true;
//     return true;
// }

// /**
//  * @brief Main loop tick: pumps USB, shows menu on connect, handles input.
//  */
// void cli_core_task(void) {
//     if (!s_ready) return;

//     tud_task();

//     bool now_connected = tud_cdc_connected();
//     if (now_connected && !s_was_connected) {
//         menu_init_root();
//     }
//     s_was_connected = now_connected;
//     if (!now_connected) return;

//     if (current_mode == CLI_MODE_MENU) {
//         int c = getchar_timeout_us(0);
//         if (c < 0) return;
//         if (c <= ' ') return;
//         if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
//         menu_input((char)c);
//     }
// }
bool cli_core_init(void) {
    // 1) Register transport & commands
    cli_command_init();

    // 2) Install default banner if none provided
    static const char *default_banner[] = {
        "=== Project Console ===",
        "Version:    1.0.0",
        "Built:     " __DATE__,
        "Platform:   RP2040"
    };
    if (!s_banner_lines) {
        s_banner_lines = default_banner;
        s_banner_count = sizeof default_banner / sizeof *default_banner;
    }

    // 3) Show the root menu (banner waits on first connect)
    menu_init_root();

    // 4) Mark ready & enabled
    s_ready       = true;
    s_cli_enabled = true;
    return true;
}


void cli_core_task(void) {
    if (!s_ready) return;

    tud_task();
    bool now_connected = tud_cdc_connected();

    if (now_connected && !s_was_connected) {
        if (!s_banner_shown) {
            cli_print_banner();
            s_banner_shown = true;
        }
        menu_init_root();
    }
    s_was_connected = now_connected;
    if (!now_connected) return;

    // If disabled, watch for key but skip exactly one
    if (!s_cli_enabled) {
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (s_skip_once) {
                // consume this one and reset
                s_skip_once = false;
            } else {
                // real re-enable
                cli_core_enable();
            }
        }
        return;
    }

    // Normal menu input
    if (current_mode == CLI_MODE_MENU) {
        int c = getchar_timeout_us(0);
        if (c < 0 || c <= ' ') return;
        if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        menu_input((char)c);
    }
}

bool cli_core_is_ready(void) {
    return s_ready && tud_cdc_connected();
}

bool cli_core_is_enabled(void) {
    return s_cli_enabled;
}

void cli_core_set_mode(cli_mode_t new_mode) {
    current_mode = new_mode;
}

void cli_core_disable(void) {
    s_cli_enabled = false;
    s_skip_once   = true;         // skip the next keypress
    printf("Leaving CLI mode. USB stays up. Press any key to re-enter.\n\n");
}

void cli_core_enable(void) {
    s_cli_enabled = true;
    cli_print_banner();
    menu_init_root();
}

#endif /* ENABLE_CLI */