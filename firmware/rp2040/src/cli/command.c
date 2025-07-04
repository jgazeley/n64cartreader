//command.c
#include "cli/command.h"
#include "cli/core.h"

#include <stdio.h>
#include <string.h>    // for memcpy(), strcmp()
#include <stdlib.h>

#include "hardware/watchdog.h"
#include "pico/stdlib.h"       // for tight_loop_contents()

static const cli_command_t *g_table = NULL;
static size_t              g_count = 0;

// Forward‐declare the core handlers
static bool cmd_help    (const cli_args_t *args);
static bool cmd_exit    (const cli_args_t *args);
static bool cmd_version (const cli_args_t *args);
static bool cmd_uptime  (const cli_args_t *args);
static bool cmd_mem     (const cli_args_t *args);
static bool cmd_ping    (const cli_args_t *args);
static bool cmd_banner  (const cli_args_t *args);
static bool cmd_pin     (const cli_args_t *args);

// The array of commands built into the CLI:
static const cli_command_t core_cmds[] = {
    // { "help",    cmd_help,     "Show this list of commands" },
    { "exit",    cmd_exit,     "Disconnect USB and leave CLI until re-connect" },
    // { "version", cmd_version,  "Print firmware version and build date" },
    // { "uptime",  cmd_uptime,   "Show time since last reset (ms)" },
    // { "mem",     cmd_mem,      "Report free/used RAM" },
    // { "ping",    cmd_ping,     "Respond with “pong”" },
    // { "banner",  cmd_banner,   "Reprint the CLI banner" },
    // { "pin",     cmd_pin,      "GPIO helper: pin R|W <num> [value]" },
};

void cli_command_init(void) {
    // Register the core commands
    cli_command_register(core_cmds,
                         sizeof core_cmds / sizeof *core_cmds);
}

void cli_command_register(const cli_command_t *cmds, size_t count) {
    size_t new_count = g_count + count;
    g_table = realloc((void*)g_table, new_count * sizeof *g_table);
    memcpy((void*)&g_table[g_count], cmds, count * sizeof *cmds);
    g_count = new_count;
}

bool cli_command_lookup(const char *name, const cli_command_t **out_cmd) {
    for (size_t i = 0; i < g_count; i++) {
        if (strcmp(g_table[i].name, name) == 0) {
            *out_cmd = &g_table[i];
            return true;
        }
    }
    return false;
}

bool cli_command_execute(const cli_command_t *cmd, const cli_args_t *args) {
    if (!cmd || !cmd->fn)   return false;
    return cmd->fn(args);
}

bool cli_command_run(const char *name, const cli_args_t *args) {
    const cli_command_t *cmd;
    if (!cli_command_lookup(name, &cmd)) {
        printf("Unknown command: %s\n", name);
        return false;
    }
    return cli_command_execute(cmd, args);
}

void cli_command_list(const cli_command_t **out_cmds, size_t *out_count) {
    *out_cmds   = g_table;
    *out_count  = g_count;
}

//------------------------------------------------------------------------------
// Core command handlers
//------------------------------------------------------------------------------

static bool cmd_help(const cli_args_t *args) {
    (void)args;
    const cli_command_t *table;
    size_t count;
    cli_command_list(&table, &count);

    printf("Available commands:\n");
    for (size_t i = 0; i < count; i++) {
        printf("  %-10s  %s\n",
               table[i].name,
               table[i].help ? table[i].help : "");
    }
    return true;
}

// RP2040 Hard Reboot
static bool cmd_exit(const cli_args_t *args) {
    (void)args;
    printf("Rebooting system…\n");
    // full MCU reset → USB detaches & re-attaches
    watchdog_reboot(0, 0, 0);
    while (1) tight_loop_contents();
    return true; // unreachable
}

// static bool cmd_exit(const cli_args_t *args) {
//     (void)args;
//     // Soft-exit CLI loop, just like Ctrl-C
//     // tud_disconnect();
//     cli_core_disable();
//     return true;
// }

// void exit_cmd_register(void) {
//     static const cli_command_t exit_cmd = {
//         .name = "exit",
//         .fn   = cmd_exit,            // ← match the struct member
//         .help = "Disconnect USB and leave CLI until re-connect"
//     };
//     cli_command_register(&exit_cmd, 1);
// }

// ─────────────────────────────────────────────────────────────────────────────
// Stub implementations
// ─────────────────────────────────────────────────────────────────────────────

/**
 * cmd_version()
 *  - Prints a version string and build date.
 */
static bool cmd_version(const cli_args_t *args) {
    (void)args;
    printf("Firmware version: 1.0.0 (built: " __DATE__ ")\n");
    return true;
}

/**
 * cmd_uptime()
 *  - Shows milliseconds since last reset.
 */
static bool cmd_uptime(const cli_args_t *args) {
    (void)args;
    // TODO: replace with your platform’s millis() or to_ms_since_boot()
    printf("Uptime: <not yet implemented>\n");
    return true;
}

/**
 * cmd_mem()
 *  - Reports free vs. used heap/RAM.
 */
static bool cmd_mem(const cli_args_t *args) {
    (void)args;
    // TODO: probe heap or call malloc_stats()
    printf("Memory: <not yet implemented>\n");
    return true;
}

/**
 * cmd_ping()
 *  - Quick link test: prints “pong”.
 */
static bool cmd_ping(const cli_args_t *args) {
    (void)args;
    printf("pong\n");
    return true;
}

/**
 * cmd_banner()
 *  - Reprints the CLI banner (useful after clearing the screen).
 */
static bool cmd_banner(const cli_args_t *args) {
    (void)args;
    cli_print_banner();
    return true;
}

/**
 * cmd_pin()
 *  - GPIO helper:  
 *      pin R <num>    → read GPIO<num>  
 *      pin W <num> <v>→ write GPIO<num> = v  
 */
static bool cmd_pin(const cli_args_t *args) {
    // TODO: parse args->argc/argv, init gpio, and call gpio_get/put()
    printf("pin: <not yet implemented>\n");
    return true;
}