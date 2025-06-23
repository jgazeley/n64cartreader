/* ui.c – hierarchical CLI over TinyUSB CDC */
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "tusb.h"

#include <app/cli.h>
#include <bus/ad_bus.h>
#include <bus/joybus.h>
#include <devices/cartridge.h>

/* ------------------------------------------------------------ */
/*  Menu actions                                                */
/* ------------------------------------------------------------ */
static void cli_rom_dump   (void){ printf("\n(stub) Dump Rom\r\n"); }
static void cli_save_read  (void){ printf("\n(stub) Read Save\r\n"); }
static void cli_save_write (void){ printf("\n(stub) Write Save\r\n"); }
static void cli_test_ctrl  (void){ printf("\n(stub) Test Controller\r\n"); }
static void cli_mpk_read   (void){ printf("\n(stub) Read MPK\r\n"); }
static void cli_mpk_write  (void){ printf("\n(stub) Write MPK\r\n"); }
static void cli_gameshark  (void){ printf("\n(stub) Gameshark\r\n"); }
static void cli_repro      (void){ printf("\n(stub) Repro Tool\r\n"); }
static void cli_reset_pico (void){ printf("\n(stub) Reset Pico\r\n"); }

/* ------------------------------------------------------------ */
/*  Menu definition                                             */
/* ------------------------------------------------------------ */
typedef void (*menu_fn_t)(void);
typedef struct { char key; const char *txt; menu_fn_t fn; } menu_t;

/* ---------- Debug Menu ---------- */
static const menu_t menu_dbg[] = {
    {'1', "Dump Header", dbg_display_header},
    {'2', "Print Rom Name", dbg_display_title},
    {'3', "Ping (SRAM)", dbg_ping_sram},
    {'4', "Ping (EEPROM)", dbg_ping_eep},
    {'5', "Write (SRAM)", dbg_write_sram},
    {'6', "Dump SRAM to Stdout", dbg_dump_sram},
    {'b', "Back",      NULL}
};
#define DBG_COUNT (sizeof menu_dbg / sizeof menu_dbg[0])

/* ---------- Main Menu ---------- */
static const menu_t menu_root[] = {
    {'1', "Game Cartridge", menu_cartridge},
    {'2', "Controller",     menu_controller},
    {'3', "Advanced",         menu_extras},
    {'4', "Reset Pico",     cli_reset_pico},
};
#define ROOT_COUNT (sizeof menu_root / sizeof menu_root[0])

/* ---------- Sub: Cartridge ---------- */
static const menu_t menu_cart[] = {
    {'1', "Dump ROM",    cli_rom_dump},
    {'2', "Read Save",   cli_save_read},
    {'3', "Write Save",  cli_save_write},
    {'b', "Back",        NULL}           /* NULL ⇒ pop menu */
};
#define CART_COUNT (sizeof menu_cart / sizeof menu_cart[0])

/* ---------- Sub: Controller ---------- */
static const menu_t menu_ctrl[] = {
    {'1', "Test Controller", cli_test_ctrl},
    {'2', "Read MPK",        cli_mpk_read},
    {'3', "Write MPK",       cli_mpk_write},
    {'b', "Back",            NULL}
};
#define CTRL_COUNT (sizeof menu_ctrl / sizeof menu_ctrl[0])

/* ---------- Sub: Extras ---------- */
static const menu_t menu_ext[] = {
    {'1', "Gameshark", cli_gameshark},
    {'2', "Repro",     cli_repro},
    {'3', "Debug",     menu_debug },
    {'b', "Back",      NULL}
};
#define EXT_COUNT (sizeof menu_ext / sizeof menu_ext[0])

/* ------------------------------------------------------------ */
/*  Simple menu stack                                           */
/*     depth 0 → root, 1 → 1st sub-menu, 2 → 2nd sub-menu …     */
/* ------------------------------------------------------------ */
#define MENU_STACK_DEPTH  3      /* room for root + 2 sub-levels */
#define MENU_STACK_TOP   (MENU_STACK_DEPTH - 1)

typedef struct {
    const menu_t *tbl;
    size_t        count;
    const char   *title;
} menu_ctx_t;

static menu_ctx_t ctx_stack[MENU_STACK_DEPTH];
static int        sp = 0;        /* “stack pointer” points at current ctx */

/* Push/pop helpers --------------------------------------------------- */
static inline void push_menu(const menu_t *tbl,
                             size_t        cnt,
                             const char   *title)
{
    if (sp < MENU_STACK_TOP)
        ctx_stack[++sp] = (menu_ctx_t){ tbl, cnt, title };
    else
        printf("\n! Menu stack overflow\n");
}

static inline void pop_menu(void)
{
    if (sp > 0) --sp;
}

/* Current menu convenience macro ------------------------------------ */
#define CUR  (ctx_stack[sp])

/* ------------------------------------------------------------ */
/*  Display current menu                                        */
/* ------------------------------------------------------------ */
static void show_menu(void)
{
    printf("\n========== %s ==========\r\n", CUR.title);
    for (size_t i = 0; i < CUR.count; ++i)
        printf("[%c] %s\r\n", CUR.tbl[i].key, CUR.tbl[i].txt);
    printf("----------------------------------\r\n> ");
    fflush(stdout);
}

/* ------------------------------------------------------------ */
/*  Top-level wrappers that push sub-menus                       */
/* ------------------------------------------------------------ */
static inline void menu_cartridge (void)
{ push_menu(menu_cart, CART_COUNT, "Game Cartridge"); }

static inline void menu_controller(void)
{ push_menu(menu_ctrl, CTRL_COUNT, "Controller");     }

static inline void menu_extras    (void)
{ push_menu(menu_ext, EXT_COUNT,  "Advanced");          }

static inline void menu_debug    (void)
{ push_menu(menu_dbg, DBG_COUNT,  "Debug");          }

/* ------------------------------------------------------------ */
/*  Public task – call from super-loop                          */
/* ------------------------------------------------------------ */
void cli_task(void)
{
    /* 1. Detect a (re)connection of a terminal ------------------------ */
    static bool was_connected = false;
    bool now_connected = tud_cdc_connected();

    if (now_connected && !was_connected)
    {
        /* reset to the root menu each time a new terminal opens */
        sp = 0;
        ctx_stack[0] =
        #ifndef DEBUG
            (menu_ctx_t){ menu_root, ROOT_COUNT, "n64-pico CLI" };
        #else
            (menu_ctx_t){ menu_dbg, DBG_COUNT, "n64-pico Debug CLI" };
        #endif            
        show_menu();
    }
    was_connected = now_connected;

    if (!now_connected) return;          /* nothing to do until connected */

    /* 2. Drain all pending characters without blocking ----------------- */
    int ch;
    while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT)
    {
        /* CR / LF just refresh the prompt */
        if (ch == '\r' || ch == '\n') {
            show_menu();
            continue;
        }

        /* Search current menu */
        bool matched = false;
        for (size_t i = 0; i < CUR.count; ++i)
        {
            if (ch == CUR.tbl[i].key)
            {
                if (CUR.tbl[i].fn)
                    CUR.tbl[i].fn();     /* action or push submenu */
                else
                    pop_menu();          /* entry with NULL fn = Back */
                matched = true;
                break;
            }
        }

        if (!matched)
            printf("\n? unknown choice '%c'\r\n", ch);

        show_menu();                     /* redraw current menu */
    }
}

/* ------------------------------------------------------------ */
/*  Debug menu functions                                        */
/* ------------------------------------------------------------ */
static void dbg_display_title(void) {
    char title[21];
    if (n64_get_title((uint8_t*)title, sizeof(title))) {
        printf("ROM Title: \"%s\"\n", title);
    } else {
        printf("ROM Title: \"%s\" (invalid)\n", title);
    }
}

static void dbg_display_header(void) {
    uint8_t header[N64_HEADER_LENGTH];
    if (!n64_get_header(header, sizeof(header))) {
        printf("Error: Failed to read N64 header\n");
        return;
    }

    #define BYTES_PER_ROW 16

    printf("\nN64 Header @ 0x%06X (%u bytes):\n", (unsigned)N64_ROM_BASE, (unsigned)N64_HEADER_LENGTH);
    for (size_t row = 0; row < N64_HEADER_LENGTH; row += BYTES_PER_ROW) {
        // Print the offset
        printf("%06X: ", (unsigned)(N64_ROM_BASE + row));

        // Print hex bytes
        for (size_t col = 0; col < BYTES_PER_ROW; ++col) {
            printf("%02X ", header[row + col]);
        }

        // Print ASCII column
        printf(" |");
        for (size_t col = 0; col < BYTES_PER_ROW; ++col) {
            uint8_t b = header[row + col];
            putchar(isprint(b) ? b : '.');
        }
        printf("|\n");
    }
}

static void print_hex_buffer(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        // at the start of each 16-byte line, print the offset
        if (i % 16 == 0) {
            printf("\n%04zx: ", i);
        }
        printf("%02X ", buf[i]);
    }
    printf("\n");
}

static void dbg_ping_sram(void) {
    //1) Read the first 512 bytes of SRAM (256 words)
    uint8_t Buffer[512];
    for (uint16_t i = 0; i < 256; ++i) {
        uint32_t addr = N64_SRAM_BASE + i*2;
        uint16_t w    = sram_read_word(addr);
        Buffer[i*2    ] = (uint8_t)(w >> 8);
        Buffer[i*2 + 1] = (uint8_t)(w & 0xFF);
    }

    // 2) Grab the cart title for a header
    char    title[N64_TITLE_LENGTH + 1];
    bool    got = n64_get_title((uint8_t*)title, sizeof(title));

    // 3) Print header
    printf("%s.sra contents:\n", got ? title : "NOCART");

    // 4) Hex-dump the 512-byte buffer
    print_hex_buffer(Buffer, sizeof(Buffer));
}

static void dbg_ping_eep(void) {
    uint8_t Buffer[512];
    char    title[ N64_TITLE_LENGTH + 1 ];
    bool    got;

    ReadEepromData(0, Buffer);

    // Fill `title[]`; got == true if a valid title was read
    got = n64_get_title((uint8_t*)title, sizeof(title));

    // Now print the buffer, not the bool
    printf("%s.eep contents:\n",
           got ? title : "NOCART");

    print_hex_buffer(Buffer, sizeof(Buffer));
}

static void dbg_dump_sram(void) {
    dump_sram_to_stdio();
}

static void dbg_write_sram(void) {
    write_first_32_bytes();
}