file(TO_CMAKE_PATH "${REPOSITORY_PATH}" repository_path)

set(
  initialize_payload
  "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"repository_path\":\"${repository_path}\"}}"
)
set(shutdown_payload "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\",\"params\":{}}")
set(exit_payload "{\"jsonrpc\":\"2.0\",\"method\":\"exit\",\"params\":{}}")
string(LENGTH "${initialize_payload}" initialize_length)
string(LENGTH "${shutdown_payload}" shutdown_length)
string(LENGTH "${exit_payload}" exit_length)

file(
  WRITE "${INPUT_PATH}"
  "Content-Length: ${initialize_length}\r\n\r\n${initialize_payload}"
)
file(
  APPEND "${INPUT_PATH}"
  "Content-Length: ${shutdown_length}\r\n\r\n${shutdown_payload}"
)
file(APPEND "${INPUT_PATH}" "Content-Length: ${exit_length}\r\n\r\n${exit_payload}")

execute_process(
  COMMAND "${SHERPA_SERVER}"
  INPUT_FILE "${INPUT_PATH}"
  OUTPUT_FILE "${OUTPUT_PATH}"
  ERROR_VARIABLE server_error
  RESULT_VARIABLE server_status
)
if(NOT server_status EQUAL 0)
  message(FATAL_ERROR "sherpa-server failed (${server_status}): ${server_error}")
endif()

file(READ "${OUTPUT_PATH}" server_output)
if(NOT server_output MATCHES "\"protocol_version\":1")
  message(FATAL_ERROR "sherpa-server did not return protocol version 1: ${server_output}")
endif()
if(NOT server_output MATCHES "\"id\":2,\"jsonrpc\":\"2.0\",\"result\":null")
  message(FATAL_ERROR "sherpa-server did not complete shutdown: ${server_output}")
endif()
