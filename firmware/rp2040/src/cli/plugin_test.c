// ─────────────────────────────────────────────────────────────────────────────
// File: src/cli/plugin_test.c
// Description: “Test” plugin for filling, dumping, and writing to a buffer.
// ─────────────────────────────────────────────────────────────────────────────

#include "cli/command.h"
#include "cli/menu.h"
#include "cli/plugins.h"
#include "cli/plugin_test.h"

#include "utils/format.h"
#include "io.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TEST_BUF_SIZE 256

//------------------------------------------------------------------------------
// Static Data
//------------------------------------------------------------------------------

static uint8_t s_test_buffer[TEST_BUF_SIZE];

//------------------------------------------------------------------------------
// Forward declarations for private command handlers
//------------------------------------------------------------------------------

static bool cmd_test_fill_buffer   (const cli_args_t *args);
static bool cmd_test_dump_buffer   (const cli_args_t *args);
static bool cmd_test_write_message (const cli_args_t *args);
static bool cmd_test_clear_buffer  (const cli_args_t *args);
static bool cmd_test_search_buffer (const cli_args_t *args);
static bool cmd_test_echo          (const cli_args_t *args);

//------------------------------------------------------------------------------
// Command Registration
//------------------------------------------------------------------------------

static void init_test_commands(void) {
    static const cli_command_t test_cmds[] = {
        { "fillbuf",   cmd_test_fill_buffer,   "Fill buffer with 0x00–0xFF pattern" },
        { "dumpbuf",   cmd_test_dump_buffer,   "Hex-dump the test buffer"           },
        { "writemsg",  cmd_test_write_message, "Write a fixed test message"         },
        { "clearbuf",  cmd_test_clear_buffer,  "Clear the test buffer to zeros"     },
        { "searchbuf", cmd_test_search_buffer, "Prompt & search buffer for string"  },
        { "echo",      cmd_test_echo,          "Prompt & echo back user text"       },
    };
    cli_command_register(test_cmds, sizeof(test_cmds)/sizeof(*test_cmds));
}

void plugin_test_register(void) {
    cli_register_command_init(init_test_commands);
}

//------------------------------------------------------------------------------
// Menu Definition
//------------------------------------------------------------------------------

static const menu_item_t test_menu_items[] = {
    { 'F', "Fill Buffer",   "fillbuf",   NULL },
    { 'D', "Dump Buffer",   "dumpbuf",   NULL },
    { 'W', "Write Message", "writemsg",  NULL },
    { 'C', "Clear Buffer",  "clearbuf",  NULL },
    { 'S', "Search Buffer", "searchbuf", NULL },
    { 'E', "Echo Text",     "echo",      NULL },
    { 'B', "Back",          NULL,        NULL },
};

const menu_frame_t test_menu_frame = {
    .title = "Debug Tests",
    .items = test_menu_items,
    .count = sizeof(test_menu_items) / sizeof(*test_menu_items)
};

const menu_frame_t * const g_test_menu = &test_menu_frame;

//------------------------------------------------------------------------------
// Command Handlers
//------------------------------------------------------------------------------

/**
 * cmd_test_fill_buffer()
 *  - Fill s_test_buffer with incremental bytes 0x00, 0x01, … 0xFF.
 */
static bool cmd_test_fill_buffer(const cli_args_t *args) {
    (void)args;
    for (size_t i = 0; i < TEST_BUF_SIZE; i++) {
        s_test_buffer[i] = (uint8_t)i;
    }
    printf("Test buffer filled (0…%u)\n", TEST_BUF_SIZE - 1);
    return true;
}

/**
 * cmd_test_dump_buffer()
 *  - Hex-dump the entire test buffer to the console.
 */
static bool cmd_test_dump_buffer(const cli_args_t *args) {
    (void)args;
    hexdump_options_t opts = {
        .bytes_per_line = 16,
        .max_rows       = 0,    // unlimited
        .show_ascii     = true,
    };
    utils_format_hexdump_ex(
        s_test_buffer,
        TEST_BUF_SIZE,
        0x00000000,  // base address label
        &opts
    );
    return true;
}

/**
 * cmd_test_write_message()
 *  - Write a fixed string into the start of s_test_buffer.
 */
static bool cmd_test_write_message(const cli_args_t *args) {
    (void)args;
    const char *msg = "Mika, Taz, Dash, Patch, Daisy (Doo), Shadow";
    size_t len = strlen(msg);

    if (len >= TEST_BUF_SIZE) {
        len = TEST_BUF_SIZE - 1;
    }

    memcpy(s_test_buffer, msg, len);
    s_test_buffer[len] = '\0';

    printf("Wrote message: \"%s\"\n", msg);
    return true;
}



/**
 * cmd_test_clear_buffer()
 *  - Zero out the entire test buffer.
 */
static bool cmd_test_clear_buffer(const cli_args_t *args) {
    (void)args;
    memset(s_test_buffer, 0, TEST_BUF_SIZE);
    printf("Test buffer cleared.\n");
    return true;
}

/**
 * cmd_test_search_buffer()
 *  - Prompt the user for a string, then search and report its offset.
 */
static bool cmd_test_search_buffer(const cli_args_t *args) {
    (void)args;
    char line[64];
    size_t idx = 0;

    // Prompt & echo input
    cli_io_write_str("Enter search string: ");
    while (idx + 1 < sizeof(line)) {
        cli_io_poll();
        int c = cli_io_read_char_nonblocking();
        if (c < 0) continue;
        char ch = (char)c;
        if (ch == '\r' || ch == '\n') {
            cli_io_write_str("\r\n");
            break;
        }
        cli_io_write_char(ch);
        line[idx++] = ch;
    }
    line[idx] = '\0';

    if (idx == 0) {
        printf("No string entered.\n");
        return true;
    }

    // Search buffer
    printf("Searching for \"%s\"...\n", line);
    char *found = strstr((char*)s_test_buffer, line);
    if (found) {
        printf("Found at offset 0x%X\n", (unsigned)(found - (char*)s_test_buffer));
    } else {
        printf("Not found.\n");
    }
    return true;
}

/**
 * cmd_test_echo()
 *  - Simple echo prompt: read a line, echo back as one string.
 */
static bool cmd_test_echo(const cli_args_t *args) {
    (void)args;
    char buf[128];
    size_t idx = 0;

    cli_io_write_str("Enter text to echo: ");
    while (idx + 1 < sizeof(buf)) {
        cli_io_poll();
        int c = cli_io_read_char_nonblocking();
        if (c < 0) continue;
        char ch = (char)c;
        if (ch == '\r' || ch == '\n') {
            cli_io_write_str("\r\n");
            break;
        }
        cli_io_write_char(ch);
        buf[idx++] = ch;
    }
    buf[idx] = '\0';

    printf("%s\n", buf);
    return true;
}
