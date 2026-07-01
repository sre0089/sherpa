file(REMOVE "${DATABASE_PATH}")

execute_process(
  COMMAND "${SHERPA_EXECUTABLE}" index "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "sherpa index failed (${result}): ${error}")
endif()

if(NOT output MATCHES "Indexed 3 source files")
  message(FATAL_ERROR "unexpected output: ${output}")
endif()

if(NOT output MATCHES "3 added, 0 modified, 0 unchanged, 0 deleted" OR
   NOT output MATCHES "Parsed: 3, reused: 0" OR
   NOT output MATCHES "Timing: [0-9]+ ms total")
  message(FATAL_ERROR "unexpected incremental metrics: ${output}")
endif()

if(NOT output MATCHES "Symbols: 4")
  message(FATAL_ERROR "unexpected symbol output: ${output}")
endif()

if(NOT output MATCHES "Includes: 2")
  message(FATAL_ERROR "unexpected include output: ${output}")
endif()

if(NOT output MATCHES "Relationships: 6 \\(6 resolved, 0 ambiguous, 0 unresolved\\)")
  message(FATAL_ERROR "unexpected relationship output: ${output}")
endif()

if(NOT EXISTS "${DATABASE_PATH}")
  message(FATAL_ERROR "database was not created")
endif()

execute_process(
  COMMAND "${SHERPA_EXECUTABLE}" index "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
  RESULT_VARIABLE incremental_result
  OUTPUT_VARIABLE incremental_output
  ERROR_VARIABLE incremental_error
)
if(NOT incremental_result EQUAL 0)
  message(
    FATAL_ERROR
    "incremental sherpa index failed (${incremental_result}): ${incremental_error}"
  )
endif()
if(NOT incremental_output MATCHES "0 added, 0 modified, 3 unchanged, 0 deleted" OR
   NOT incremental_output MATCHES "Parsed: 0, reused: 3")
  message(FATAL_ERROR "unexpected unchanged index output: ${incremental_output}")
endif()
