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
    "${SHERPA_EXECUTABLE}" callees run --repo "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
  RESULT_VARIABLE callees_result
  OUTPUT_VARIABLE callees_output
  ERROR_VARIABLE callees_error
)
if(NOT callees_result EQUAL 0)
  message(FATAL_ERROR "sherpa callees failed (${callees_result}): ${callees_error}")
endif()
if(NOT callees_output MATCHES "target \\[resolved, high\\]")
  message(FATAL_ERROR "resolved callee missing: ${callees_output}")
endif()
if(NOT callees_output MATCHES "overloaded \\[ambiguous, high\\]")
  message(FATAL_ERROR "ambiguous callee missing: ${callees_output}")
endif()
if(NOT callees_output MATCHES "missing \\[unresolved, low\\]")
  message(FATAL_ERROR "unresolved callee missing: ${callees_output}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" callers target --repo "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
    --format json
  RESULT_VARIABLE callers_result
  OUTPUT_VARIABLE callers_json
  ERROR_VARIABLE callers_error
)
if(NOT callers_result EQUAL 0)
  message(FATAL_ERROR "sherpa callers failed (${callers_result}): ${callers_error}")
endif()
string(JSON query_kind GET "${callers_json}" query)
string(JSON schema_version GET "${callers_json}" schema_version)
string(JSON callers_ok GET "${callers_json}" ok)
string(JSON call_count LENGTH "${callers_json}" calls)
string(JSON caller_name GET "${callers_json}" calls 0 caller qualified_name)
if(NOT schema_version EQUAL 1 OR
   NOT callers_ok OR
   NOT query_kind STREQUAL "callers" OR
   NOT call_count EQUAL 1 OR
   NOT caller_name STREQUAL "run")
  message(FATAL_ERROR "unexpected callers JSON: ${callers_json}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" callees overloaded --repo "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
  RESULT_VARIABLE ambiguous_result
  ERROR_VARIABLE ambiguous_error
)
if(NOT ambiguous_result EQUAL 6 OR NOT ambiguous_error MATCHES "symbol is ambiguous")
  message(FATAL_ERROR "unexpected ambiguous-symbol result: ${ambiguous_result}: ${ambiguous_error}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" callers unknown --repo "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
  RESULT_VARIABLE missing_result
  ERROR_VARIABLE missing_error
)
if(NOT missing_result EQUAL 5 OR NOT missing_error MATCHES "symbol not found")
  message(FATAL_ERROR "unexpected missing-symbol result: ${missing_result}: ${missing_error}")
endif()
