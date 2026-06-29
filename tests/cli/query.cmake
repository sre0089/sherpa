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
string(JSON schema_version GET "${file_json}" schema_version)
string(JSON file_ok GET "${file_json}" ok)
string(JSON file_path GET "${file_json}" path)
string(JSON definition_count LENGTH "${file_json}" definitions)
string(JSON outgoing_count LENGTH "${file_json}" outgoing_includes)
if(NOT schema_version EQUAL 1 OR
   NOT file_ok OR
   NOT query_kind STREQUAL "file" OR
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
    "${DATABASE_PATH}" --format json
  RESULT_VARIABLE missing_result
  OUTPUT_VARIABLE missing_json
  ERROR_VARIABLE missing_error
)
string(JSON missing_ok GET "${missing_json}" ok)
string(JSON missing_code GET "${missing_json}" error code)
string(JSON missing_candidates LENGTH "${missing_json}" error candidates)
if(NOT missing_result EQUAL 5 OR
   missing_ok OR
   NOT missing_code STREQUAL "not_found" OR
   NOT missing_candidates EQUAL 0 OR
   NOT missing_error STREQUAL "")
  message(FATAL_ERROR "unexpected missing-file result: ${missing_result}: ${missing_json}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" query symbol overloaded --repo "${FIXTURE_DIR}" --database
    "${DATABASE_PATH}" --format json
  RESULT_VARIABLE ambiguous_result
  OUTPUT_VARIABLE ambiguous_json
  ERROR_VARIABLE ambiguous_error
)
string(JSON ambiguous_code GET "${ambiguous_json}" error code)
string(JSON ambiguous_candidates LENGTH "${ambiguous_json}" error candidates)
if(NOT ambiguous_result EQUAL 6 OR
   NOT ambiguous_code STREQUAL "ambiguous_symbol" OR
   NOT ambiguous_candidates EQUAL 2 OR
   NOT ambiguous_error STREQUAL "")
  message(FATAL_ERROR "unexpected ambiguity result: ${ambiguous_result}: ${ambiguous_json}")
endif()

execute_process(
  COMMAND "${SHERPA_EXECUTABLE}" query symbol --format json
  RESULT_VARIABLE usage_result
  OUTPUT_VARIABLE usage_json
  ERROR_VARIABLE usage_error
)
string(JSON usage_code GET "${usage_json}" error code)
if(NOT usage_result EQUAL 2 OR
   NOT usage_code STREQUAL "invalid_usage" OR
   NOT usage_error STREQUAL "")
  message(FATAL_ERROR "unexpected usage result: ${usage_result}: ${usage_json}: ${usage_error}")
endif()
