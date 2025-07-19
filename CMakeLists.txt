cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(triggerSweeper)

# set(PICO_USE_FASTEST_SUPPORTED_CLOCK 1) # 200 MHz
add_compile_definitions(SYS_CLK_MHZ=200)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(triggerSweeper
    main.c
    pulsegen.pio
    trigger_test.pio
)

pico_generate_pio_header(triggerSweeper ${CMAKE_CURRENT_LIST_DIR}/pulsegen.pio)
pico_generate_pio_header(triggerSweeper ${CMAKE_CURRENT_LIST_DIR}/trigger_test.pio)

pico_enable_stdio_usb(triggerSweeper 1)
pico_enable_stdio_uart(triggerSweeper 0)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(triggerSweeper pico_stdlib hardware_pio hardware_dma)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(triggerSweeper)