#include "stdio.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "trigger_sweeper.pio.h"


#define TRIGGER_PIN 0
#define OUTPUT_PIN 1

uint pio_programm_offset;

void update_delay(PIO pio, uint sm, uint delay_count) {
    // More thorough state machine reset
    pio_sm_set_enabled(pio, sm, false);

    // Clear both TX and RX FIFOs
    pio_sm_clear_fifos(pio, sm);

    // Reset the state machine to initial state
    pio_sm_restart(pio, sm);

    // Clear any stale data in OSR
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_null, 32));

    // Push new delay value
    pio_sm_put(pio, sm, delay_count-1);

    // Important: Re-initialize the state machine's program counter
    pio_sm_exec(pio, sm, pio_encode_jmp(pio_programm_offset));

    // Restart the state machine
    pio_sm_set_enabled(pio, sm, true);
}

int main() {
    // Set delay_count (number of cycles that output should be high)
    uint delay_count = 10;

    stdio_init_all();

    // Load PIO program
    PIO pio = pio0;
    pio_programm_offset = pio_add_program(pio, &trigger_sweeper_program);
    uint sm = 0;

    // Configure pin directions
    pio_sm_set_consecutive_pindirs(pio, sm, OUTPUT_PIN, 1, true);  // output pin
    gpio_pull_down(TRIGGER_PIN);
    pio_gpio_init(pio, TRIGGER_PIN);   // input trigger pin
    pio_gpio_init(pio, OUTPUT_PIN);    // output trigger pin

    // Set input and sideset bases
    pio_sm_config c = trigger_sweeper_program_get_default_config(pio_programm_offset);
    sm_config_set_in_pins(&c, TRIGGER_PIN);
    sm_config_set_sideset_pins(&c, OUTPUT_PIN);
    gpio_set_drive_strength(OUTPUT_PIN, GPIO_DRIVE_STRENGTH_12MA);
    sm_config_set_clkdiv(&c, 1.0f);  // Full speed: 125MHz (8ns per instr)

    // Initialize and enable state machine
    pio_sm_init(pio, sm, pio_programm_offset, &c);

    // Push delay_value to TX FIFO (which loads OSR before enabling SM)
    pio_sm_put(pio, sm, delay_count-1);

    // Start state machine
    pio_sm_set_enabled(pio, sm, true);

    while (1) {
       // Check for incoming serial data
        int cmd = getchar_timeout_us(0);
        if (cmd != PICO_ERROR_TIMEOUT) {
            if (cmd == 'G') {
                // Get current delay
                printf("%u\n", delay_count);
            } else if (cmd == 'S') {
                // Set new delay - read 4 bytes
                uint new_delay = 0;
                for (int i = 0; i < 4; i++) {
                    int b = getchar_timeout_us(100000); // 100ms timeout per byte
                    if (b == PICO_ERROR_TIMEOUT) {
                        printf("Timeout reading delay value\n");
                        break;
                    }
                    new_delay |= (b & 0xFF) << (i * 8);
                }

                if (new_delay != delay_count) {
                    delay_count = new_delay;
                    update_delay(pio, sm, delay_count);
                }
                printf("OK\n");
            } else {
                printf("Unknown command '%c'\n", cmd);
            }
        }

        // Your other main loop code here
        tight_loop_contents();
    }
}
