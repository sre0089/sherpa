file(REMOVE "${DATABASE_PATH}")

execute_process(
  COMMAND "${SHERPA_EXECUTABLE}" index "${FIXTURE_DIR}" --database "${DATABASE_PATH}"
  RESULT_VARIABLE index_result
  OUTPUT_VARIABLE index_output
  ERROR_VARIABLE index_error
)
if(NOT index_result EQUAL 0)
  message(FATAL_ERROR "Python fixture indexing failed (${index_result}): ${index_error}")
endif()
if(NOT index_output MATCHES "Indexed 6 source files")
  message(FATAL_ERROR "mixed-language index summary missing: ${index_output}")
endif()

execute_process(
  COMMAND "${SHERPA_EXECUTABLE}" query symbol "app.utils::helper"
          --repo "${FIXTURE_DIR}" --database "${DATABASE_PATH}" --format json
  RESULT_VARIABLE symbol_result
  OUTPUT_VARIABLE symbol_output
  ERROR_VARIABLE symbol_error
)
if(NOT symbol_result EQUAL 0)
  message(FATAL_ERROR "Python symbol query failed (${symbol_result}): ${symbol_error}")
endif()
if(NOT symbol_output MATCHES "\"qualified_name\":\"app.utils::helper\"")
  message(FATAL_ERROR "Python qualified symbol missing: ${symbol_output}")
endif()
if(NOT symbol_output MATCHES "\"file\":\"app/utils.py\"")
  message(FATAL_ERROR "Python symbol file missing: ${symbol_output}")
endif()

execute_process(
  COMMAND "${SHERPA_EXECUTABLE}" query callees "app.service::Service::twice"
          --repo "${FIXTURE_DIR}" --database "${DATABASE_PATH}" --format json
  RESULT_VARIABLE callees_result
  OUTPUT_VARIABLE callees_output
  ERROR_VARIABLE callees_error
)
if(NOT callees_result EQUAL 0)
  message(FATAL_ERROR "Python callees query failed (${callees_result}): ${callees_error}")
endif()
string(REGEX MATCHALL "\"qualified_name\":\"app.service::Service::process\""
       process_matches "${callees_output}")
list(LENGTH process_matches process_count)
if(NOT process_count EQUAL 2)
  message(FATAL_ERROR "expected two resolved Python method calls: ${callees_output}")
endif()
