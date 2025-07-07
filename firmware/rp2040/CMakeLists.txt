cmake_minimum_required(VERSION 3.17)

# ── SDK import ─────────────────────────────────────────────────────
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico-pak C CXX ASM)

set(PICO_BOARD pico)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ── Build Options ──────────────────────────────────────────────────
option(ENABLE_CLI "Build with the CLI shell enabled" ON)
option(ENABLE_MSC_MODE "Build with USB Mass Storage enabled" OFF)

if(ENABLE_CLI)
    add_compile_definitions(ENABLE_CLI=1)
    message(STATUS "Building with CLI Mode ENABLED")
elseif(ENABLE_MSC_MODE)
    add_compile_definitions(ENABLE_MSC_MODE=1)
    message(STATUS "Building with USB Mass Storage (MSC) Mode ENABLED")
else()
    message(FATAL_ERROR "No build mode selected. Please enable either ENABLE_CLI or ENABLE_MSC_MODE.")
endif()


# ── Executable + sources ───────────────────────────────────────────
add_executable(pico-pak
    # --- Core application logic (always included) ---
    src/main.c
    src/n64/bus/adbus.c
    src/n64/bus/joybus.c
    src/n64/devices/gamepak.c
    src/utils/crc.c
    src/utils/format.c
    src/usb/filesystem.c

    # --- CLI layer (only included if ENABLE_CLI is ON) ---
    $<$<BOOL:${ENABLE_CLI}>:src/cli/core.c>
    $<$<BOOL:${ENABLE_CLI}>:src/cli/menu.c>
    $<$<BOOL:${ENABLE_CLI}>:src/cli/menu_definitions.c>
    $<$<BOOL:${ENABLE_CLI}>:src/cli/command.c>
    $<$<BOOL:${ENABLE_CLI}>:src/cli/plugins.c>
    $<$<BOOL:${ENABLE_CLI}>:src/cli/plugin_n64.c>
    $<$<BOOL:${ENABLE_CLI}>:src/cli/io_pico.c>

    # --- USB MSC files (only included if ENABLE_MSC_MODE is ON) ---
    $<$<BOOL:${ENABLE_MSC_MODE}>:src/usb/usb_descriptors.c>
    $<$<BOOL:${ENABLE_MSC_MODE}>:src/usb/msc_disk.c>
    $<$<BOOL:${ENABLE_MSC_MODE}>:src/usb/filesystem.c>
)

# ── Include Directories ────────────────────────────────────────────
target_include_directories(pico-pak PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    # Conditionally add the usb include path for MSC mode
    $<$<BOOL:${ENABLE_MSC_MODE}>:${CMAKE_CURRENT_SOURCE_DIR}/src/usb>
)

# ── PIO Generation ─────────────────────────────────────────────────
pico_generate_pio_header(pico-pak ${CMAKE_CURRENT_LIST_DIR}/src/n64/bus/joybus.pio)

# ── Compile Definitions ────────────────────────────────────────────
# These definitions are common to all modes
target_compile_definitions(pico-pak PRIVATE
    CFG_TUSB_MCU=OPT_MCU_RP2040
    CFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE
    PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
)

# ── Std-IO selection ───────────────────────────────────────────────
pico_enable_stdio_usb(pico-pak 1)
pico_enable_stdio_uart(pico-pak 0)

# ── Libraries ──────────────────────────────────────────────────────
target_link_libraries(pico-pak PRIVATE
    pico_stdlib
    hardware_pio

    # Conditionally link the full TinyUSB device stack for MSC mode
    # The default stdio lib is enough for the CLI's serial port.
    $<$<BOOL:${ENABLE_MSC_MODE}>:tinyusb_device>
    $<$<BOOL:${ENABLE_MSC_MODE}>:tinyusb_board>
)

# ── Extra artefacts (UF2 / bin / hex / map) ────────────────────────
pico_add_extra_outputs(pico-pak)

# ── Custom Flash Target ──────────────────────────────────────────
find_program(PICOTOOL_PATH picotool)
if(PICOTOOL_PATH)
    add_custom_target(flash
        COMMAND ${PICOTOOL_PATH} reboot -f -u
        COMMAND ${CMAKE_COMMAND} -E sleep 2
        COMMAND ${PICOTOOL_PATH} load -f "$<TARGET_FILE_DIR:pico-pak>/pico-pak.uf2"
        COMMAND ${PICOTOOL_PATH} reboot
        DEPENDS pico-pak
        COMMENT "Flashing pico-pak with picotool"
    )
endif()