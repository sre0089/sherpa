file(REMOVE "${DATABASE_PATH}")

execute_process(
  COMMAND "${SHERPA_EXECUTABLE}" index "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
  RESULT_VARIABLE index_result
  ERROR_VARIABLE index_error
)
if(NOT index_result EQUAL 0)
  message(FATAL_ERROR "sherpa index failed (${index_result}): ${index_error}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" path run leaf --repo "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
  RESULT_VARIABLE text_result
  OUTPUT_VARIABLE text_output
  ERROR_VARIABLE text_error
)
if(NOT text_result EQUAL 0)
  message(FATAL_ERROR "sherpa path failed (${text_result}): ${text_error}")
endif()
if(NOT text_output MATCHES "confirmed \\(3 steps\\)" OR
   NOT text_output MATCHES "run --calls--> top" OR
   NOT text_output MATCHES "middle --calls--> leaf")
  message(FATAL_ERROR "unexpected path output: ${text_output}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" path maybe alpha::candidate --repo "${FIXTURE_DIR}"
    --database "${DATABASE_PATH}" --format json
  RESULT_VARIABLE json_result
  OUTPUT_VARIABLE path_json
  ERROR_VARIABLE json_error
)
if(NOT json_result EQUAL 0)
  message(FATAL_ERROR "sherpa path JSON failed (${json_result}): ${json_error}")
endif()
string(JSON query_kind GET "${path_json}" query)
string(JSON schema_version GET "${path_json}" schema_version)
string(JSON path_ok GET "${path_json}" ok)
string(JSON path_status GET "${path_json}" status)
string(JSON step_count LENGTH "${path_json}" steps)
string(JSON step_resolution GET "${path_json}" steps 0 resolution)
if(NOT schema_version EQUAL 1 OR
   NOT path_ok OR
   NOT query_kind STREQUAL "path" OR
   NOT path_status STREQUAL "possible" OR
   NOT step_count EQUAL 1 OR
   NOT step_resolution STREQUAL "ambiguous")
  message(FATAL_ERROR "unexpected path JSON: ${path_json}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" path leaf run --repo "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
  RESULT_VARIABLE disconnected_result
  OUTPUT_VARIABLE disconnected_output
)
if(NOT disconnected_result EQUAL 0 OR NOT disconnected_output MATCHES "not_found")
  message(
    FATAL_ERROR
    "unexpected disconnected result: ${disconnected_result}: ${disconnected_output}"
  )
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" path unknown leaf --repo "${FIXTURE_DIR}"
    --database "${DATABASE_PATH}"
  RESULT_VARIABLE missing_result
  ERROR_VARIABLE missing_error
)
if(NOT missing_result EQUAL 5 OR NOT missing_error MATCHES "source symbol not found")
  message(FATAL_ERROR "unexpected missing-symbol result: ${missing_result}: ${missing_error}")
endif()
