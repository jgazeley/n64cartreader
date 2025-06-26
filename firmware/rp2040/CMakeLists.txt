cmake_minimum_required(VERSION 3.17)

# ── SDK import ─────────────────────────────────────────────────────
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(n64_dumper C CXX ASM)

set(PICO_BOARD pico)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

  # ── Build Options ───────────────────────────────────────────────────────────
  option(ENABLE_CLI "Build with the CLI shell enabled" ON)
  if(ENABLE_CLI)
    add_compile_definitions(ENABLE_CLI=1)
  endif()

# ── Executable + sources ───────────────────────────────────────────
add_executable(n64_dumper

#    ── Platform layer (hardware I/O) ────────────────────────────
    #TODO

    # ── Core application logic ───────────────────────────────────
    src/main.c
    src/n64/bus/adbus.c
    src/n64/bus/joybus.c
    # src/n64/devices/gamepak.c
    # src/n64/devices/controller.c

    # ── Utilities ────────────────────────────────────────────────
    src/utils/crc.c
    src/utils/format.c
    src/utils/packet.c
    src/utils/transport.c

    # # ── CLI layer (only if ENABLE_CLI) ───────────────────────────
    $<$<BOOL:${ENABLE_CLI}>:src/cli/core.c>
    $<$<BOOL:${ENABLE_CLI}>:src/cli/menu.c>
    $<$<BOOL:${ENABLE_CLI}>:src/cli/command.c>
    $<$<BOOL:${ENABLE_CLI}>:src/cli/cmd_test.c>
)

# Generate the PIO header for the "n64_dumper" target.
pico_generate_pio_header(n64_dumper ${CMAKE_CURRENT_LIST_DIR}/src/n64/bus/joybus.pio)

# make the generated PIO headers visible
target_include_directories(n64_dumper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src     # so "bus/generated/..." in your source tree is found
    ${CMAKE_CURRENT_BINARY_DIR}/src     # so generated PIO headers are found
)

target_compile_definitions(n64_dumper PRIVATE
    # Make sure TinyUSB knows CDC is enabled
    CFG_TUD_ENABLED=1
    CFG_TUD_CDC=1
    CFG_TUD_CDC_RX_BUFSIZE=8192
    CFG_TUD_CDC_TX_BUFSIZE=8192
    CFG_TUSB_MCU=OPT_MCU_RP2040
    CFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE

    # Toggle CRC Fast (Table), CRC Slow (Bitwise) is default 
    USE_CRC_FAST=0
)

# If you want to disable the CLI at compile time:
#   cmake -DENABLE_CLI=OFF .

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

# ── Custom Flash Target ──────────────────────────────────────────────

# Find the picotool executable, which is the standard flashing utility.
# You may need to build this from the pico-tools repository first.
find_program(PICOTOOL_PATH picotool)

if(PICOTOOL_PATH)
    add_custom_target(flash
        # 1. Ask the board to reboot into BOOTSEL (USB) mode.
        COMMAND ${PICOTOOL_PATH} reboot -f -u
        
        # 2. Add a short delay to give Windows time to see the board reappear.
        COMMAND ${CMAKE_COMMAND} -E sleep 2
        
        # 3. Load the UF2 file. The -f flag helps find the board if it was already in BOOTSEL mode.
        COMMAND ${PICOTOOL_PATH} load -f "$<TARGET_FILE_DIR:n64_dumper>/n64_dumper.uf2"
        
        # 4. Reboot the board again to run the new program.
        COMMAND ${PICOTOOL_PATH} reboot
        
        DEPENDS n64_dumper
        COMMENT "Flashing n64_dumper with picotool (Windows method)"
        )
else()
    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo "Could not find picotool. Please build it and ensure it's in your PATH."
        )
endif()