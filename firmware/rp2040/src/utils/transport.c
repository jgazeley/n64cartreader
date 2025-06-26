/*  ===========================================================================
 *  utils/transport.c
 *  ---------------------------------------------------------------------------
 *  A tiny “backend selector” + TinyUSB-CDC implementation that the higher-level
 *  packet/CLI layers call through.  If you later add a second transport
 *  (UART, MSC-bulk, etc.) just drop another xxx_backend struct alongside
 *  `cdc_backend` and call `transport_register(&xxx_backend)` instead.
 *  ===========================================================================
 */
#include "utils/transport.h"

#include <stddef.h>
#include <stdbool.h>
#include "tusb.h"                 /* TinyUSB device API                     */

/* --------------------------------------------------------------------------
 *  Registry helpers
 * ------------------------------------------------------------------------ */
static const transport_t *s_current = NULL;

void transport_register(const transport_t *t)
{
    s_current = t;
}

const transport_t *transport_get(void)
{
    return s_current;
}

/* --------------------------------------------------------------------------
 *  CDC backend
 * ------------------------------------------------------------------------ */
#ifndef CFG_TUD_CDC
#error "TinyUSB CDC class must be enabled (CFG_TUD_CDC=1)"
#endif

/*  Write the buffer in bursts that fit the device TX FIFO.
 *  `tud_task()` is called whenever we need to wait, so the USB stack
 *  keeps servicing IN/OUT tokens and never stalls.                         */
static bool cdc_write_bytes(const uint8_t *buf, size_t len)
{
    while (len)
    {
        /* Wait until there is *some* room in the FIFO */
        while (tud_cdc_write_available() == 0)
            tud_task();                                 /* keep USB alive */

        size_t room = tud_cdc_write_available();
        if (room > len) room = len;

        /* Copy up to ‘room’ bytes into the FIFO */
        size_t n = tud_cdc_write(buf, (uint32_t)room);
        buf += n;
        len -= n;

        tud_task();               /* let TinyUSB ship the packet upstream */
    }

    tud_cdc_write_flush();         /* final ZLP / short-packet as needed   */
    return true;
}

static void cdc_flush(void)
{
    tud_cdc_write_flush();
}

/*  Non-blocking single-byte read.  Returns true if a byte was read
 *  and placed into *b, false if nothing is waiting.                       */
static bool cdc_read_byte(uint8_t *b)
{
    if (!tud_cdc_connected() || !tud_cdc_available())
        return false;

    return (tud_cdc_read(b, 1) == 1);
}

/*  CDC “vtable” ---------------------------------------------------------- */
static const transport_t cdc_backend = {
    .write_bytes = cdc_write_bytes,
    .flush       = cdc_flush,
    .read_byte   = cdc_read_byte,
};

/*  Called once from main() (before any packet/CLI code)                    */
void transport_init(void)
{
    transport_register(&cdc_backend);
}
