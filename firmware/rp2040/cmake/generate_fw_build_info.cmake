if(NOT DEFINED OUTPUT)
  message(FATAL_ERROR "OUTPUT is required")
endif()

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(TEMPLATE "${SOURCE_DIR}/include/headless/fw_build_info.h.in")
if(NOT EXISTS "${TEMPLATE}")
  message(FATAL_ERROR "Template not found: ${TEMPLATE}")
endif()

set(FW_GIT_DESCRIBE "nogit")
if(DEFINED GIT_EXECUTABLE AND NOT GIT_EXECUTABLE STREQUAL "" AND EXISTS "${GIT_EXECUTABLE}")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" describe --always --dirty --tags
    OUTPUT_VARIABLE FW_GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(FW_GIT_DESCRIBE STREQUAL "")
    set(FW_GIT_DESCRIBE "nogit")
  endif()
endif()

string(TIMESTAMP FW_BUILD_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)

get_filename_component(OUTPUT_DIR "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
configure_file("${TEMPLATE}" "${OUTPUT}" @ONLY)
