// File: src/cli/menu.c (Optimized & Commented)

#include "cli/menu.h"
#include "cli/command.h"
#include "cli/core.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_MENU_DEPTH 5  // Maximum levels of nested submenus

// A single entry on the menu navigation stack
typedef struct {
    const menu_frame_t *frame;
} menu_stack_entry_t;

// The menu stack and its stack pointer (s_sp == -1 means “empty”)
static menu_stack_entry_t s_stack[MAX_MENU_DEPTH];
static int                s_sp = -1;

/**
 * menu_init()
 *  - Initialize the stack with the root menu frame.
 *  - Sets s_sp = 0 so menu_render()/menu_input() operate on root.
 */
void menu_init(const menu_frame_t *root_frame) {
    s_sp = 0;
    s_stack[0].frame = root_frame;
}

/**
 * menu_render()
 *  - Bail out if stack is empty.
 *  - Print the current frame’s title and each menu item:
 *      • Printable ASCII keys are shown as “ X) Description”
 *      • Non-printable keys are shown in hex “0x##) Description”
 *  - Prompt “Select:” and flush stdout so the user sees it immediately.
 */
void menu_render(void) {
    if (s_sp < 0) return;                       // Nothing to render

    const menu_frame_t *f = s_stack[s_sp].frame;
    printf("\n=== %s ===\n", f->title);         // Menu title

    // List each item
    for (size_t i = 0; i < f->count; i++) {
        char key = f->items[i].key;
        if (key >= ' ' && key <= '~') {
            printf(" %c) %s\n", key, f->items[i].desc);
        } else {
            printf("0x%02X) %s\n", (uint8_t)key, f->items[i].desc);
        }
    }

    fputs("\nSelect: ", stdout);                // Prompt
    fflush(stdout);                             // Ensure prompt is shown
}

/**
 * menu_input()
 *  - Returns true if the character `ch` matched and was handled.
 *  - Early-exits if no menu is active or CLI is disabled.
 *  - Iterates the current frame’s items:
 *      • On matching key:
 *          - If cmd_name present: run it via cli_command_run()
 *          - Else if submenu present: push it onto s_stack (up to MAX_MENU_DEPTH)
 *          - Else if stack depth > 0: pop back to parent menu
 *      • Sets need_render = true if any of the above happened.
 *  - After loop, if need_render is still true: re-render menu.
 */
bool menu_input(char ch) {
    if (s_sp < 0)                 return false;   // No active menu
    // if (!cli_core_is_enabled())   return false;   // CLI disabled

    const menu_frame_t *f = s_stack[s_sp].frame;
    bool                need_render = false;

    // Find matching key in current menu items
    for (size_t i = 0; i < f->count; i++) {
        const menu_item_t *it = &f->items[i];
        if (it->key != ch) continue;

        need_render = true;

        if (it->cmd_name) {
            // Execute the associated command
            cli_command_run(it->cmd_name, NULL);
            // If command disabled the CLI, skip re-render
            // if (!cli_core_is_enabled()) {
            //     need_render = false;
            // }
        }
        else if (it->submenu) {
            // Enter submenu if room on stack
            if (s_sp + 1 < MAX_MENU_DEPTH) {
                s_stack[++s_sp].frame = it->submenu;
            }
        }
        else if (s_sp > 0) {
            // “Back” action: no cmd or submenu, so pop stack
            --s_sp;
        }
        break;  // Stop scanning items after a match
    }

    // Redraw menu only if an action occurred
    if (need_render) {
        menu_render();
        return true;
    }

    return false;  // No matching key or nothing to do
}






























































































// // File: src/cli/menu.c (Final, Cleaned Version)

// #include "cli/menu.h"
// #include "cli/command.h"
// #include "cli/core.h"

// #include <stdio.h>
// #include <stdint.h>

// #define MAX_MENU_DEPTH 5

// // A single entry on the menu navigation stack
// typedef struct {
//     const menu_frame_t *frame;
// } menu_stack_entry_t;

// // The actual menu stack and stack pointer
// static menu_stack_entry_t s_stack[MAX_MENU_DEPTH];
// static int s_sp = -1;

// // --- Core Engine Functions ---

// void menu_init(const menu_frame_t *root_frame) {
//     s_sp = 0;
//     s_stack[0].frame = root_frame;
// }

// void menu_render(void) {
//     if (s_sp < 0) return; // Nothing to render

//     const menu_frame_t *f = s_stack[s_sp].frame;
//     printf("\n=== %s ===\n", f->title);
//     for (size_t i = 0; i < f->count; i++) {
//         char key = f->items[i].key;
//         if (key >= ' ' && key <= '~') {
//             printf(" %c) %s\n", key, f->items[i].desc);
//         } else {
//             printf("0x%02X) %s\n", (uint8_t)key, f->items[i].desc);
//         }
//     }
//     printf("\nSelect: ");
//     fflush(stdout);
// }

// bool menu_input(char ch) {
//     if (s_sp < 0) return false;

//     const menu_frame_t *f = s_stack[s_sp].frame;
//     for (size_t i = 0; i < f->count; i++) {
//         const menu_item_t *it = &f->items[i];
//         if (it->key != ch) continue;

//         // A matching key was found, process the action
//         if (it->cmd_name) {
//             cli_command_run(it->cmd_name, NULL);
//             if (!cli_core_is_enabled()) return true;
//         } else if (it->submenu) {
//             if (s_sp + 1 < MAX_MENU_DEPTH) {
//                 s_stack[++s_sp].frame = it->submenu;
//             }
//         } else if (s_sp > 0) {
//             // This is the 'Back' option (key defined, but no cmd or submenu)
//             s_sp--;
//         }
        
//         menu_render();
//         return true;
//     }
//     return false; // No matching key found
// }