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

if(NOT output MATCHES "Indexed 3 C/C\\+\\+ files")
  message(FATAL_ERROR "unexpected output: ${output}")
endif()

if(NOT EXISTS "${DATABASE_PATH}")
  message(FATAL_ERROR "database was not created")
endif()
