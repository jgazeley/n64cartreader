cmake_minimum_required(VERSION 3.17)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico-pak C CXX ASM)

set(PICO_BOARD pico)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# --- 1. DEFINE PRE-BUILT CLI LIBRARY ---
# These must match your 'tree' output exactly
set(PICOCLI_LIB ${CMAKE_CURRENT_LIST_DIR}/lib/pico-cli/libpico_cli.a)
set(PICOCLI_INC ${CMAKE_CURRENT_LIST_DIR}/lib/pico-cli/include)

add_executable(pico-pak
  src/main.c
  src/cli/menu_definitions.c
  src/cli/plugin_n64.c
  src/n64/bus/adbus.c
  src/n64/bus/joybus.c
  src/n64/devices/gamepak.c
)

# --- UPDATE THIS SECTION ---
target_include_directories(pico-pak PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${PICOCLI_INC}    # <--- This now points to lib/pico-cli/include
  ${CMAKE_CURRENT_BINARY_DIR}
)

# Generate PIO header
pico_generate_pio_header(pico-pak ${CMAKE_CURRENT_LIST_DIR}/src/n64/bus/joybus.pio)

# --- 3. LINK AGAINST THE ARCHIVE ---
target_link_libraries(pico-pak PRIVATE
  ${PICOCLI_LIB}     # <--- Points to lib/pico-cli/libpico_cli.a
  pico_stdlib
  hardware_pio
  hardware_gpio
)

target_compile_definitions(pico-pak PRIVATE
  PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
)

pico_enable_stdio_usb(pico-pak 1)
pico_enable_stdio_uart(pico-pak 0)

pico_add_extra_outputs(pico-pak)

# Add a 'make flash' shortcut
add_custom_target(flash
    COMMAND picotool load -f pico-pak.uf2 -x
    DEPENDS pico-pak
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Flashing Pico..."
)