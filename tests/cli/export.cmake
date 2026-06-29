file(REMOVE "${DATABASE_PATH}")
file(REMOVE "${OUTPUT_PATH}")

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
    "${SHERPA_EXECUTABLE}" export "${OUTPUT_PATH}" --repo "${FIXTURE_DIR}"
    --database "${DATABASE_PATH}"
  RESULT_VARIABLE export_result
  OUTPUT_VARIABLE export_output
  ERROR_VARIABLE export_error
)
if(NOT export_result EQUAL 0)
  message(FATAL_ERROR "sherpa export failed (${export_result}): ${export_error}")
endif()
if(NOT export_output MATCHES "Exported 20 nodes and 26 edges")
  message(FATAL_ERROR "unexpected export output: ${export_output}")
endif()

file(READ "${OUTPUT_PATH}" graph_json)
string(JSON schema_version GET "${graph_json}" schema_version)
string(JSON generator GET "${graph_json}" generator name)
string(JSON repository_root GET "${graph_json}" repository root)
string(JSON node_count LENGTH "${graph_json}" nodes)
string(JSON edge_count LENGTH "${graph_json}" edges)
if(NOT schema_version EQUAL 1 OR
   NOT generator STREQUAL "sherpa" OR
   NOT repository_root STREQUAL "." OR
   NOT node_count EQUAL 20 OR
   NOT edge_count EQUAL 26)
  message(FATAL_ERROR "unexpected graph export contract: ${graph_json}")
endif()
file(SHA256 "${OUTPUT_PATH}" first_hash)

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" export "${OUTPUT_PATH}" --repo "${FIXTURE_DIR}"
    --database "${DATABASE_PATH}"
  RESULT_VARIABLE exists_result
  ERROR_VARIABLE exists_error
)
if(NOT exists_result EQUAL 7 OR NOT exists_error MATCHES "use --force")
  message(FATAL_ERROR "unexpected existing-output result: ${exists_result}: ${exists_error}")
endif()

execute_process(
  COMMAND
    "${SHERPA_EXECUTABLE}" export "${OUTPUT_PATH}" --repo "${FIXTURE_DIR}"
    --database "${DATABASE_PATH}" --force
  RESULT_VARIABLE force_result
  ERROR_VARIABLE force_error
)
if(NOT force_result EQUAL 0)
  message(FATAL_ERROR "forced export failed (${force_result}): ${force_error}")
endif()
file(SHA256 "${OUTPUT_PATH}" second_hash)
if(NOT first_hash STREQUAL second_hash)
  message(FATAL_ERROR "graph export is not deterministic")
endif()

file(GLOB temporary_files "${OUTPUT_PATH}.sherpa-tmp-*")
if(temporary_files)
  message(FATAL_ERROR "temporary export files were not cleaned up: ${temporary_files}")
endif()
