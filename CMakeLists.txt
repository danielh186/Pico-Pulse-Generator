cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(triggerSweeper)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(triggerSweeper
    main.c
    trigger_sweeper.pio
)

pico_generate_pio_header(triggerSweeper ${CMAKE_CURRENT_LIST_DIR}/trigger_sweeper.pio)

pico_enable_stdio_usb(triggerSweeper 1)
pico_enable_stdio_uart(triggerSweeper 0)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(triggerSweeper pico_stdlib hardware_pio)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(triggerSweeper)