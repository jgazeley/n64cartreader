/*  main.c – RP2040 / Pico
 *  ---------------------------------------------------------------
 *  • Single CDC ACM port (stdio-USB) – VID 2E8A, PID 000A
 *  • Works with `picotool reboot/load` (no BOOTSEL button)
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "tusb.h"

#include <app/cli.h>
#include <bus/ad_bus.h>
#include <bus/joybus.h>

/*------------------------------------------------------------------*/
/* Main                                                             */
/*------------------------------------------------------------------*/
int main(void)
{
    stdio_init_all();        // routes printf to USB CDC (adds vendor iface)
    tusb_init();             // TinyUSB device stack

    // Initialize AD Bus and Joybus
    n64_adBus_init();
    n64_eep_init();

    while (true)
    {
        tud_task();          // TinyUSB polling
        cli_task();           // CLI
    }
}