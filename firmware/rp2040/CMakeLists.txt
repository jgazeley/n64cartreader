cmake_minimum_required(VERSION 3.17)

# ── SDK import ─────────────────────────────────────────────────────
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(n64_dumper C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ── Executable + sources ───────────────────────────────────────────
add_executable(n64_dumper
    src/app/main.c
    src/app/cli.c
    src/bus/ad_bus.c
    src/bus/joybus.c
    src/devices/cartridge.c
    src/devices/controller.c)

# Generate the PIO header for the "n64_dumper" target.
pico_generate_pio_header(n64_dumper ${CMAKE_CURRENT_LIST_DIR}/src/bus/joybus.pio)

target_include_directories(n64_dumper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Make sure TinyUSB knows CDC is enabled
target_compile_definitions(n64_dumper PRIVATE
    CFG_TUD_ENABLED=1
    CFG_TUD_CDC=1
    CFG_TUD_CDC_RX_BUFSIZE=256    # pick any power-of-two you like
    CFG_TUD_CDC_TX_BUFSIZE=256
    CFG_TUSB_MCU=OPT_MCU_RP2040
    CFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE)

target_compile_options(n64_dumper PRIVATE -Wno-error)

# Let the SDK compile its vendor-reset helper for picotool
add_compile_definitions(PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1)

# ── Std-IO selection ───────────────────────────────────────────────
pico_enable_stdio_usb (n64_dumper 1)
pico_enable_stdio_uart(n64_dumper 0)

# ── Libraries ──────────────────────────────────────────────────────
target_link_libraries(n64_dumper PRIVATE
    pico_stdlib
    tinyusb_board
    hardware_pio
)

# ── Extra artefacts (UF2 / bin / hex / map) ────────────────────────
pico_add_extra_outputs(n64_dumper)

