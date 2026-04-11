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
    COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse --short=7 HEAD
    OUTPUT_VARIABLE FW_GIT_DESCRIBE
    RESULT_VARIABLE FW_GIT_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(NOT FW_GIT_RESULT EQUAL 0 OR FW_GIT_DESCRIBE STREQUAL "")
    set(FW_GIT_DESCRIBE "nogit")
  else()
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" diff --quiet --ignore-submodules HEAD --
      RESULT_VARIABLE FW_GIT_DIRTY_RESULT
      OUTPUT_QUIET
      ERROR_QUIET
    )
    if(FW_GIT_DIRTY_RESULT EQUAL 1)
      string(APPEND FW_GIT_DESCRIBE "-dirty")
    endif()
  endif()
endif()

string(TIMESTAMP FW_BUILD_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)

get_filename_component(OUTPUT_DIR "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
configure_file("${TEMPLATE}" "${OUTPUT}" @ONLY)
