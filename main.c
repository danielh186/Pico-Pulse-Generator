#include "stdio.h"
#include "stdlib.h"
#include "ctype.h"
#include "string.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "pulsegen.pio.h"
#include "trigger_test.pio.h"


#define TRIGGER_PIN 0
#define PULSE_PIN 1
#define TEST_PIN 5

#define ENABLE_TEST_PIN_PIO
// #define ENABLE_TEST_PIN_LOOP // makes serial configuration unresponsive
#define TEST_PIN_HIGH_CYCLES 10
#define TEST_PIN_LOW_CYCLES 10

int dma_chan; // dma channel (set inside setup_dma and needed in dma_irq_handler())

/**
 * @brief DMA interrupt handler to automatically restart a DMA transfer.
 *
 * This function is called when the DMA channel finishes its transfer (i.e., the transfer
 * count is exhausted). It clears the interrupt flag for the associated DMA channel and
 * restarts the transfer. This creates the effect of an "infinite" DMA loop when used
 * with a fixed source and destination.
 *
 * Note: This function assumes that `dma_chan` is a global variable specifying the DMA channel used.
 */
void dma_irq_handler() {
    // Clear interrupt request
    dma_hw->ints0 = 1u << dma_chan;
    dma_channel_start(dma_chan);
}


/**
 * @brief Sets up a DMA channel to continuously send a constant 32-bit value to a PIO TX FIFO.
 *
 * This function configures a DMA channel to repeatedly transfer a single 32-bit word
 * (pointed to by `fifo_value`) into the TX FIFO of the specified PIO state machine.
 *
 * The transfer is throttled using the PIO TX FIFO DREQ signal. An interrupt handler is set
 * up to auto-restart the DMA when the transfer completes (simulating infinite transfer).
 *
 * @param pio         The PIO instance (e.g., pio0 or pio1).
 * @param sm          The state machine index (0–3) to send data to.
 * @param fifo_value  Pointer to a 32-bit value to be sent repeatedly to the PIO TX FIFO.
 */
void setup_dma(PIO pio, uint sm, const uint32_t *fifo_value) {
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_cfg, false); // Always write the same word from buffer (first word)
    channel_config_set_write_increment(&dma_cfg, false); // Do not increment target addr (always write to same PIO FIFO addr)
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, true));  // Throttle by PIO TX FIFO


    // Configure to auto-restart DMA when it finishes (number of transfers reached)
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_configure(
        dma_chan,
        &dma_cfg,
        &pio->txf[sm],         // dest: PIO FIFO
        fifo_value,           // src: ring buffer
        0xFFFFFFFF,           // number of transfers
        false                 // don't start yet
    );

    dma_channel_start(dma_chan);
}


/**
 * @brief Packs three separate parameters into a single 32-bit word.
 *
 * This function combines three values — `repeats`, `spacing`, and `length` —
 * into a single `uint32_t` by assigning specific bit fields:
 *
 * - `repeats`: 5 bits  (bits 0–4)
 * - `length`:  7 bits  (bits 5–11)
 * - `spacing`: 20 bits (bits 12–31)
 *
 * @param repeats   Number of repetitions (max 31).
 * @param spacing   Time or space between operations (max 1048576).
 * @param length    Length or size indicator (max 127).
 * @return          A 32-bit word encoding all three parameters.
 */
uint32_t pack_combined_parameters(uint repeats, uint spacing, uint length) {
    // Ensure inputs are within valid bit sizes
    if (repeats > 0x1F) repeats = 0x1F;         // 5 bits max: 31
    if (spacing > 0x1FFFFF) spacing = 0x1FFFFF; // 20 bits max: 1048575
    if (length > 0x7F) length = 0x7F;           // 7 bits max: 127

    return (spacing << 12) | (length << 5) | repeats;
}


/**
 * @brief Updates delay parameters and restarts the PIO state machine.
 *
 * This function fully resets the specified PIO state machine, clears its FIFOs and OSR,
 * then pushes new combined timing parameters and multiple copies of the offset value
 * into the TX FIFO (often to be consumed by DMA or the PIO program). Finally, it
 * resets the program counter and restarts the state machine.
 *
 * @param pio       The PIO instance (pio0 or pio1).
 * @param sm        The state machine number (0–3).
 * @param offset    Offset value to be pushed multiple times into the FIFO.
 * @param length    Length parameter for timing configuration.
 * @param spacing   Spacing parameter for timing configuration.
 * @param repeats   Repeat count for timing configuration.
 */
void update_delay(PIO pio, uint sm, uint program_offset, uint32_t offset, uint length, uint spacing, uint repeats) {
    // More thorough state machine reset
    pio_sm_set_enabled(pio, sm, false);

    // Clear both TX and RX FIFOs
    pio_sm_clear_fifos(pio, sm);

    // Reset the state machine to initial state
    pio_sm_restart(pio, sm);

    // Clear any stale data in OSR
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_null, 32));

    uint32_t combined = pack_combined_parameters(repeats, spacing, length); // Combine parameters into single word
    pio_sm_put(pio, sm, combined); // Put combined parameters into FIFO
    // Prefill FIFO with offset (continued automatically by DMA)
    pio_sm_put(pio, sm, offset);
    pio_sm_put(pio, sm, offset);
    pio_sm_put(pio, sm, offset);
    pio_sm_put(pio, sm, offset);
    pio_sm_put(pio, sm, offset);
    pio_sm_put(pio, sm, offset);
    pio_sm_put(pio, sm, offset);

    // Re-initialize the state machine's program counter
    pio_sm_exec(pio, sm, pio_encode_jmp(program_offset));

    // Restart the state machine
    pio_sm_set_enabled(pio, sm, true);
}

void init_pulsegen_pio(PIO pio, uint sm, uint program_offset) {
    // Configure pin directions
    gpio_pull_down(TRIGGER_PIN);
    pio_gpio_init(pio, TRIGGER_PIN);   // input trigger pin
    pio_gpio_init(pio, PULSE_PIN);    // output trigger pin
    gpio_set_drive_strength(PULSE_PIN, GPIO_DRIVE_STRENGTH_12MA);

    // Set input and sideset bases
    pio_sm_config c = pulsegen_program_get_default_config(program_offset);
    // sm_config_set_in_pins(&c, TRIGGER_PIN);
    // sm_config_set_out_pins(&c, PULSE_PIN, 1);
    pio_sm_set_consecutive_pindirs(pio, sm, PULSE_PIN, 1, true);  // output pin
    sm_config_set_sideset_pins(&c, PULSE_PIN);
    sm_config_set_clkdiv(&c, 1.0f);  // Full speed: 125MHz (8ns per instr)

    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX); // Join both FIFOs into one 8 word TX FIFO (cpu -> pio)

    pio_sm_init(pio, sm, program_offset, &c); // Initialize state machine
}

void init_test_trigger_pio(PIO pio, uint sm, uint program_offset, uint32_t high_cycles, uint32_t low_cycles) {
    pio_sm_config c = trigger_test_program_get_default_config(program_offset);

    pio_sm_clear_fifos(pio, sm);
    sm_config_set_clkdiv(&c, 200.0f);  // Full speed: 125MHz (8ns per instr)

    pio_gpio_init(pio, TEST_PIN); // Init TEST_PIN GPIO

    sm_config_set_set_pins(&c, TEST_PIN, 1); // Set output pin
    pio_sm_set_consecutive_pindirs(pio, sm, TEST_PIN, 1, true);  // Set as output

    pio_sm_init(pio, sm, program_offset, &c); // Initialize state machine

    // Push high and low cycles
    pio_sm_put(pio, sm, high_cycles);
    pio_sm_put(pio, sm, low_cycles);

    pio_sm_set_enabled(pio, sm, true); // Enable state machine
}

int main() {
    // Pulse generator configuration parameters
    uint32_t offset = 10;
    unsigned int repeats = 2;
    unsigned int length = 25;
    unsigned int spacing = 20;

    stdio_init_all();

    // Load PIO program
    PIO pulsegen_pio = pio0;
    uint pulsegen_program_offset = pio_add_program(pulsegen_pio, &pulsegen_program);
    uint pulsegen_sm = 0;

    init_pulsegen_pio(pulsegen_pio, pulsegen_sm, pulsegen_program_offset);


    setup_dma(pulsegen_pio, pulsegen_sm, &offset);
    update_delay(pulsegen_pio, pulsegen_sm, pulsegen_program_offset, offset, length, spacing, repeats);


    #if defined(ENABLE_TEST_PIN_PIO)
        PIO test_pio = pio1;
        uint test_program_offset = pio_add_program(test_pio, &trigger_test_program);
        uint test_sm = 1;


        init_test_trigger_pio(test_pio, test_sm, test_program_offset, TEST_PIN_HIGH_CYCLES - 2, TEST_PIN_LOW_CYCLES - 5);
    #elif defined(ENABLE_TEST_PIN_LOOP)
        gpio_init(TEST_PIN);
        gpio_set_dir(TEST_PIN, GPIO_OUT);

        while (true) {
            gpio_put(TEST_PIN, 1);  // Turn LED ON
            __asm__ __volatile__ (
    ".rept 200\n\t"
    "nop\n\t"
    ".endr\n\t"
);
            // sleep_us(1);         // Delay 500 ms
            gpio_put(TEST_PIN, 0);  // Turn LED OFF
            __asm__ __volatile__ (
    ".rept 200\n\t"
    "nop\n\t"
    ".endr\n\t"
);

            // sleep_us(2);         // Delay 500 ms
        }
    #endif

    typedef enum {
        CMD_GET = 0,
        CMD_SET = 1
    } CommandType;

    CommandType command;
    int read_char;
    char param_key;
    bool comm_error = false;
    int separator_detected = false;
    char val_buf[12];

    while (1) {
        if (comm_error) {
            printf("   NOK\n");
            // Flush RX Buffer (optional)
            while (uart_is_readable(uart0)) {
                (void)uart_getc(uart0);  // discard character
            }

        }
        comm_error = false;
        read_char = getchar_timeout_us(0);  // wait up to 100ms for first char ('G' or 'S')
        if (read_char == PICO_ERROR_TIMEOUT) { // No UART input, do other stuff
            tight_loop_contents();
            // Currently no incoming UART command, put other looping code here (TODO: for future functionality in main loop)
            continue;
        }
        else if ((char)read_char == 'G') {
            command = CMD_GET;
        }
        else if (read_char == 'S') {
            command = CMD_SET;
        }
        else {
            comm_error = true;
            continue;
        }

        read_char = getchar_timeout_us(100000);
        if (read_char != ' ') {// Command has to be followed by a space
            printf("TIMEOUT");
            comm_error = true;
            continue;
        }

        // GET command
        if (command == CMD_GET) {
            // Get parameter key
            param_key = (char)getchar_timeout_us(100000);
            if (param_key == PICO_ERROR_TIMEOUT) {
                comm_error = true;
                continue;
            }
            if (param_key == 'o') { // offset
                printf("%u\n", offset);
            } else if (param_key == 'l') { // length
                printf("%u\n", length);
            } else if (param_key == 's') { // spacing
                printf("%u\n", spacing);
            } else if (param_key == 'r') { // repeats
                printf("%u\n", repeats);
            } else {
                comm_error = true;
                continue;
            }
        }
        // SET command
        else if (command == CMD_SET) {
            // Copy current values
            uint32_t new_offset  = offset;
            uint32_t new_length  = length;
            uint32_t new_spacing = spacing;
            uint32_t new_repeats = repeats;
            while (1) {
                // Get parameter key
                read_char = getchar_timeout_us(100000);
                param_key = (char)read_char;
                if (read_char == PICO_ERROR_TIMEOUT) {
                    // Last parameter key reached -> break
                    break;
                }

                if (getchar_timeout_us(100000) != ' ') { // Separator between param_key and param_value (' ')
                    comm_error = true;
                    break;
                }

                // Read parameter
                if (param_key == 'o' || param_key == 'l' || param_key == 's' || param_key == 'r') {
                    separator_detected = false;
                    memset(val_buf, 0, sizeof(val_buf)); // Zeroize value buffer
                    // Read value
                    for (size_t i = 0; i < 12; i++)
                    {
                        read_char = getchar_timeout_us(900000);
                        if (read_char == PICO_ERROR_TIMEOUT) {
                            // End of command (end of last value)
                            separator_detected = true;
                            break;
                        }
                        else if (read_char == ' ') { // Space detected (end of parameter value)
                            separator_detected = true;
                            break;
                        }
                        else { // Add character to buffer it is a digit
                            if (isdigit((char)read_char)) {
                                val_buf[i] = (char)read_char;
                            }
                            else {
                                comm_error = true;
                                break;
                            }
                        }
                    }
                    if (separator_detected != true) { // Make sure that a separation char was received (passed number fits into bounds)
                        comm_error = true;
                    }
                    if (comm_error) {
                        break;
                    }

                    uint32_t val_num = (uint32_t)strtoul(val_buf, NULL, 10); // Convert string buffer to number
                    // Store parameter value into variable
                    switch (param_key) {
                        case 'o': new_offset = val_num; break;
                        case 'l': new_length = val_num; break;
                        case 's': new_spacing = val_num; break;
                        case 'r': new_repeats = val_num; break;
                        default: {
                            comm_error = true;
                            break;
                        }
                    }
                }
                else {
                    comm_error = true;
                }
                if (comm_error) {
                    break;
                }
            }
            if (comm_error) {
                continue;
            }

            // Commit all new values and update PIO
            if (new_offset < 2) {
                printf("min_offset=2");
                comm_error = true;
                continue;
            }
            if (new_length < 1) {
                printf("min_length=1");
                comm_error = true;
                continue;
            }
            if (new_spacing < 6) {
                printf("min_spacing=6");
                comm_error = true;
                continue;
            }

            offset = new_offset;
            length = new_length;
            spacing = new_spacing;
            repeats = new_repeats;
            update_delay(pulsegen_pio, pulsegen_sm, pulsegen_program_offset, offset - 2, length - 1, spacing - 6, repeats);
            printf("OK\n");
        }
    }
}
