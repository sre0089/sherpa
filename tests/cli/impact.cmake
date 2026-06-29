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
    "${SHERPA_EXECUTABLE}" impact leaf --repo "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
  RESULT_VARIABLE text_result
  OUTPUT_VARIABLE text_output
  ERROR_VARIABLE text_error
)
if(NOT text_result EQUAL 0)
  message(FATAL_ERROR "sherpa impact failed (${text_result}): ${text_error}")
endif()
if(NOT text_output MATCHES "middle.*depth 1" OR
   NOT text_output MATCHES "top.*depth 2" OR
   NOT text_output MATCHES "run.*depth 3")
  message(FATAL_ERROR "unexpected impact output: ${text_output}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" impact include/base.hpp --repo "${FIXTURE_DIR}"
    --database "${DATABASE_PATH}" --format json
  RESULT_VARIABLE json_result
  OUTPUT_VARIABLE impact_json
  ERROR_VARIABLE json_error
)
if(NOT json_result EQUAL 0)
  message(FATAL_ERROR "sherpa impact JSON failed (${json_result}): ${json_error}")
endif()
string(JSON query_kind GET "${impact_json}" query)
string(JSON schema_version GET "${impact_json}" schema_version)
string(JSON impact_ok GET "${impact_json}" ok)
string(JSON target_kind GET "${impact_json}" target kind)
string(JSON confirmed_count LENGTH "${impact_json}" confirmed)
if(NOT schema_version EQUAL 1 OR
   NOT impact_ok OR
   NOT query_kind STREQUAL "impact" OR
   NOT target_kind STREQUAL "file" OR
   NOT confirmed_count EQUAL 3)
  message(FATAL_ERROR "unexpected impact JSON: ${impact_json}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" impact unknown --repo "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
  RESULT_VARIABLE missing_result
  ERROR_VARIABLE missing_error
)
if(NOT missing_result EQUAL 5 OR NOT missing_error MATCHES "file or symbol not found")
  message(FATAL_ERROR "unexpected missing-target result: ${missing_result}: ${missing_error}")
endif()
