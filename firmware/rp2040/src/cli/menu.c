// File: src/cli/menu.c
#include "cli/menu.h"
#include "cli/command.h"
#include "cli/core.h"
#include <stdio.h>
#include <stdint.h>

#define MAX_MENU_DEPTH 5
#define MAGIC_KEY  0xA5

static const menu_frame_t *stack[MAX_MENU_DEPTH];
static int                 sp = -1;

// Debug submenu
static const menu_item_t debug_items[] = {
    { 'F', "Fill Buffer",   "fillbuf",    NULL },
    { 'D', "Dump Buffer",   "dumpbuf",    NULL },
    { 'W', "Write Message", "writemsg",   NULL },
    // { 'T', "Stream Test",   "streamtest", NULL },
    { 'I', "Get GamePak Info",   "n64_info", NULL },
    { 'N', "View N64 ROM Header",   "n64_header", NULL },
    { 'B', "Back",          NULL,         NULL },
};
const menu_frame_t debug_menu_frame = {
    .title = "Debug Tests",
    .items = debug_items,
    .count = sizeof debug_items / sizeof *debug_items
};

// Root menu (add 0xA5 entry to invoke "streambuf")
static const menu_item_t root_items[] = {
    { 'D', "Debug Tests",     NULL,               &debug_menu_frame },
    { 'H', "Help",            "help",             NULL              },
    { 'X', "Exit",            "exit",             NULL              },
};
const menu_frame_t root_menu_frame = {
    .title = "Main Menu",
    .items = root_items,
    .count = sizeof root_items / sizeof *root_items
};

void menu_init(const menu_frame_t *frame) {
    sp = 0;
    stack[0] = frame;
    menu_render();
}
void menu_init_root(void)   { menu_init(&root_menu_frame); }
void menu_init_debug(void) { menu_init(&debug_menu_frame); }

void menu_render(void) {
    if (sp < 0) return;
    const menu_frame_t *f = stack[sp];

    printf("\n=== %s ===\n", f->title);
    for (size_t i = 0; i < f->count; i++) {
        char key = f->items[i].key;
        if (key >= 32 && key <= 126) {
            printf(" %c) %s\n", key, f->items[i].desc);
        } else {
            printf("0x%02X) %s\n", (uint8_t)key, f->items[i].desc);
        }
    }
    printf("\nSelect: ");
    fflush(stdout);
}

bool menu_input(char ch) {
    // --- 0) Intercept the magic key before the normal menu lookup ---
    if ((uint8_t)ch == MAGIC_KEY) {
        // directly invoke your raw‐stream command
        cli_command_run("streambuf", NULL);
        menu_render();   // re-draw the menu so CLI UX isn’t broken
        return true;
    }

    // --- 1) Now do the normal menu dispatch (only real items) ---
    const menu_frame_t *f = stack[sp];
    for (size_t i = 0; i < f->count; i++) {
        const menu_item_t *it = &f->items[i];
        if ((uint8_t)it->key != (uint8_t)ch) continue;

    if (it->cmd_name) {
        cli_command_run(it->cmd_name, NULL);
        // if the CLI was just disabled by that command, don’t redraw
        if (!cli_core_is_enabled()) {
            return true;
        }
    } else if (it->submenu) {
            if (sp + 1 < MAX_MENU_DEPTH) stack[++sp] = it->submenu;
        } else if (sp > 0) {
            sp--;
        }
        menu_render();
        return true;
    }
    return false;
}