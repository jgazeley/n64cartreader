/* ui.h – public interface for the USB-CLI module */
#ifndef APP_CLI_H_
#define APP_CLI_H_

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG

// Menus
static void menu_cartridge(void);
static void menu_controller(void);
static void menu_extras(void);
static void menu_debug(void);

// Debug menu functions
static void dbg_display_title(void);
static void dbg_display_header(void);
static void dbg_ping_sram(void);
static void dbg_ping_eep(void);
static void dbg_dump_sram(void);
static void dbg_write_sram(void);

void cli_task(void);

#ifdef __cplusplus
}
#endif
#endif /* APP_CLI_H_ */
