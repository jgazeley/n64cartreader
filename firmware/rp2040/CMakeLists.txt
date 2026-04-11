cmake_minimum_required(VERSION 3.17)

# IMPORTANT: pico_sdk_import must happen BEFORE project() so the ARM toolchain is selected.
# If we're included from pico-pak, pico-pak already imported the SDK and set the toolchain.
if (NOT TARGET pico_stdlib)
  if (DEFINED ENV{PICO_SDK_PATH} AND EXISTS "$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
  elseif(EXISTS "$ENV{HOME}/pico-sdk/external/pico_sdk_import.cmake")
    set(PICO_SDK_PATH "$ENV{HOME}/pico-sdk")
    include($ENV{HOME}/pico-sdk/external/pico_sdk_import.cmake)
  else()
    message(FATAL_ERROR "Pico SDK not found. Set PICO_SDK_PATH or install at $HOME/pico-sdk")
  endif()
endif()

project(n64_pico_cart_reader C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Only initialize the SDK if we are the top-level project (or if pico-pak hasn't already done it)
if (NOT TARGET pico_stdlib)
  set(PICO_BOARD pico)
  pico_sdk_init()
endif()

# Build identity for host-visible firmware version query.
# Generate this header at build time so the timestamp stays fresh on
# incremental builds without requiring a full CMake reconfigure.
find_package(Git QUIET)
set(FW_BUILD_INFO_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/fw_build_info.h")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_custom_target(fw_build_info ALL
  COMMAND ${CMAKE_COMMAND}
    -DOUTPUT=${FW_BUILD_INFO_HEADER}
    -DSOURCE_DIR=${CMAKE_CURRENT_LIST_DIR}
    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
    -P ${CMAKE_CURRENT_LIST_DIR}/cmake/generate_fw_build_info.cmake
  BYPRODUCTS ${FW_BUILD_INFO_HEADER}
  COMMENT "Generating firmware build info"
  VERBATIM
)

# Headless firmware target (no CLI/menu/parser runtime)
add_executable(pico_headless_demo
  src/headless/main.c
  src/headless/protocol.c

  src/n64/bus/adbus.c
  src/n64/bus/joybus.c
  src/n64/devices/gamepak.c
  src/n64/devices/gamepak_repro.c
  src/n64/devices/gameshark.c
  src/n64/devices/controller.c

  src/utils/crc.c
  src/utils/packet.c
  src/utils/transport.c
)

add_dependencies(pico_headless_demo fw_build_info)

target_include_directories(pico_headless_demo PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(pico_headless_demo PRIVATE
  pico_stdlib
  pico_stdio_usb
  hardware_pio
  hardware_gpio
)

pico_enable_stdio_usb(pico_headless_demo 1)
pico_enable_stdio_uart(pico_headless_demo 0)
pico_add_extra_outputs(pico_headless_demo)

# Keep a simple flash shortcut for the headless image.
add_custom_target(flash
    COMMAND picotool load -f pico_headless_demo.uf2 -x
    DEPENDS pico_headless_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Flashing headless Pico firmware..."
)
