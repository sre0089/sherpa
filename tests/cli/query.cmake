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
    "${SHERPA_EXECUTABLE}" query symbol run --repo "${FIXTURE_DIR}" --database
    "${DATABASE_PATH}"
  RESULT_VARIABLE symbol_result
  OUTPUT_VARIABLE symbol_output
  ERROR_VARIABLE symbol_error
)
if(NOT symbol_result EQUAL 0)
  message(FATAL_ERROR "sherpa query symbol failed (${symbol_result}): ${symbol_error}")
endif()
if(NOT symbol_output MATCHES "Symbol run" OR
   NOT symbol_output MATCHES "Callees: 1 resolved, 1 ambiguous, 1 unresolved")
  message(FATAL_ERROR "unexpected symbol query output: ${symbol_output}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" query file src/main.cpp --repo "${FIXTURE_DIR}" --database
    "${DATABASE_PATH}" --format json
  RESULT_VARIABLE file_result
  OUTPUT_VARIABLE file_json
  ERROR_VARIABLE file_error
)
if(NOT file_result EQUAL 0)
  message(FATAL_ERROR "sherpa query file failed (${file_result}): ${file_error}")
endif()
string(JSON query_kind GET "${file_json}" query)
string(JSON file_path GET "${file_json}" path)
string(JSON definition_count LENGTH "${file_json}" definitions)
string(JSON outgoing_count LENGTH "${file_json}" outgoing_includes)
if(NOT query_kind STREQUAL "file" OR
   NOT file_path STREQUAL "src/main.cpp" OR
   NOT definition_count EQUAL 1 OR
   NOT outgoing_count EQUAL 3)
  message(FATAL_ERROR "unexpected file query JSON: ${file_json}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" query callers target --repo "${FIXTURE_DIR}" --database
    "${DATABASE_PATH}" --format json
  RESULT_VARIABLE callers_result
  OUTPUT_VARIABLE callers_json
  ERROR_VARIABLE callers_error
)
if(NOT callers_result EQUAL 0)
  message(FATAL_ERROR "sherpa query callers failed (${callers_result}): ${callers_error}")
endif()
string(JSON callers_kind GET "${callers_json}" query)
string(JSON callers_count LENGTH "${callers_json}" calls)
if(NOT callers_kind STREQUAL "callers" OR NOT callers_count EQUAL 1)
  message(FATAL_ERROR "unexpected callers JSON: ${callers_json}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" query file src/missing.cpp --repo "${FIXTURE_DIR}" --database
    "${DATABASE_PATH}"
  RESULT_VARIABLE missing_result
  ERROR_VARIABLE missing_error
)
if(NOT missing_result EQUAL 5 OR NOT missing_error MATCHES "file not found")
  message(FATAL_ERROR "unexpected missing-file result: ${missing_result}: ${missing_error}")
endif()
