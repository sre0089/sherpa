set(install_directory "${SHERPA_BINARY_DIR}/consumer-install")
set(consumer_build_directory "${SHERPA_BINARY_DIR}/consumer-build")
file(REMOVE_RECURSE "${install_directory}" "${consumer_build_directory}")

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" --install "${SHERPA_BINARY_DIR}" --prefix "${install_directory}"
    --config "${SHERPA_CONFIG}"
  RESULT_VARIABLE install_status
  OUTPUT_VARIABLE install_output
  ERROR_VARIABLE install_error
)
if(NOT install_status EQUAL 0)
  message(FATAL_ERROR "Sherpa installation failed:\n${install_output}\n${install_error}")
endif()

set(
  configure_command
  "${CMAKE_COMMAND}"
  -S "${SHERPA_SOURCE_DIR}/tests/consumer/project"
  -B "${consumer_build_directory}"
  -G "${SHERPA_GENERATOR}"
  "-DCMAKE_PREFIX_PATH=${install_directory}"
  "-DCMAKE_BUILD_TYPE=${SHERPA_CONFIG}"
  "-DVCPKG_INSTALLED_DIR=${SHERPA_BINARY_DIR}/vcpkg_installed"
)
if(NOT SHERPA_GENERATOR_PLATFORM STREQUAL "")
  list(APPEND configure_command -A "${SHERPA_GENERATOR_PLATFORM}")
endif()
if(NOT SHERPA_GENERATOR_TOOLSET STREQUAL "")
  list(APPEND configure_command -T "${SHERPA_GENERATOR_TOOLSET}")
endif()
if(NOT SHERPA_TOOLCHAIN_FILE STREQUAL "")
  list(APPEND configure_command "-DCMAKE_TOOLCHAIN_FILE=${SHERPA_TOOLCHAIN_FILE}")
endif()

execute_process(
  COMMAND ${configure_command}
  RESULT_VARIABLE configure_status
  OUTPUT_VARIABLE configure_output
  ERROR_VARIABLE configure_error
)
if(NOT configure_status EQUAL 0)
  string(REPLACE "\r" "" configure_output "${configure_output}")
  string(REPLACE "\n" " " configure_output "${configure_output}")
  string(REPLACE "\r" "" configure_error "${configure_error}")
  string(REPLACE "\n" " " configure_error "${configure_error}")
  message(FATAL_ERROR
          "Installed consumer configuration failed: ${configure_error} ${configure_output}")
endif()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" --build "${consumer_build_directory}" --config "${SHERPA_CONFIG}"
  RESULT_VARIABLE build_status
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
  message(FATAL_ERROR "Installed consumer build failed:\n${build_output}\n${build_error}")
endif()
