#include "pico/stdlib.h"
#include "utils/transport.h"
#include "headless/protocol.h"

int main(void) {
    // Keep USB CDC stack alive, but do not run CLI/menu/parser.
    stdio_init_all();
    transport_init();
    headless_protocol_init();

    while (true) {
        headless_protocol_poll();
        sleep_ms(1);
    }
}
