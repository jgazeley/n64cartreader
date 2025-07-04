/*#################################################################
######################   CONTROLLER TEST  #########################
#################################################################*/
// Debug: display N64 controller presence and accessory
// Bit reversal utility for a byte (needed if response bytes are reversed)
static uint8_t reverse_bits(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

// Assumes a 1kΩ pull-up resistor is present on the data line.
static void test_controller(void) {
    uint8_t response[3] = {0};

    // --- 1. SETUP PIO ---
    // This is a simplified, self-contained setup for this specific test.
    uint dataPin = EEP_DAT; // The pin your controller is on
    pio_gpio_init(pio, dataPin);
    uint piooffset = pio_add_program(pio, &joybus_program);
    pio_sm_config config = joybus_program_get_default_config(piooffset);
    sm_config_set_in_pins(&config, dataPin);
    sm_config_set_out_pins(&config, dataPin, 1);
    sm_config_set_set_pins(&config, dataPin, 1);
    
    // Use the correct timing for THIS PIO program
    sm_config_set_clkdiv(&config, 5);
    
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_in_shift(&config, false, true, 8); // This handles the bit-reversal
    
    printf("\nController Test Started...\n");
    
    // --- 2. SEND COMMAND ---
    uint8_t cmd[1] = {0x00}; // Identify command
    uint32_t txbuf[2];
    int txlen = 0;
    convertToPio(cmd, 1, txbuf, &txlen);

    // Start the PIO in transmit mode
    pio_sm_init(pio, 0, piooffset + joybus_offset_outmode, &config);
    pio_sm_set_enabled(pio, 0, true);
    for (int i = 0; i < txlen; i++) {
        pio_sm_put_blocking(pio, 0, txbuf[i]);
    }
    sleep_us(200); // Brief wait to ensure the last bit has been sent

    // --- 3. LISTEN FOR RESPONSE ---
    // Restart the PIO in receive mode to listen for the answer
    pio_sm_set_enabled(pio, 0, false);
    pio_sm_init(pio, 0, piooffset + joybus_offset_inmode, &config);
    pio_sm_clear_fifos(pio, 0); // Important: clear any old FIFO data
    pio_sm_set_enabled(pio, 0, true);
    
    // --- 4. READ RESPONSE WITH GENEROUS TIMEOUT ---
    bool timeout = false;
    for (int i = 0; i < 3; ++i) {
        uint32_t start_time = time_us_32();
        while (pio_sm_is_rx_fifo_empty(pio, 0)) {
            if (time_us_32() - start_time > 10000) { // Wait up to 10ms for each byte
                timeout = true;
                break;
            }
        }
        if (timeout) break;
        response[i] = (uint8_t)pio_sm_get(pio, 0);
    }

    // --- 5. SHUTDOWN PIO & SHOW RESULTS ---
    pio_sm_set_enabled(pio, 0, false);
    pio_remove_program(pio, &joybus_program, piooffset);

    if (timeout) {
        printf("Timeout: Controller not responding. Check wiring and pull-up resistor.\n");
        return;
    }

    printf("Raw Controller ID:    0x%02X\n", response[0]);
    printf("Raw Second Byte:      0x%02X\n", response[1]);
    printf("Raw Accessory/Status: 0x%02X\n", response[2]);

    uint8_t ctrl_id = response[0];
    if (ctrl_id != 0x05) {
        printf("Incorrect Controller ID received (expected 0x05).\n");
        printf("This indicates a signal integrity problem. Ensure a 1kOhm pull-up resistor is used.\n");
    } else {
        printf("Success! N64 controller detected!\n");
    }
}