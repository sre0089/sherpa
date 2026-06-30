cmake_minimum_required(VERSION 3.25)

project(
  sherpa
  VERSION 0.1.0
  DESCRIPTION "Evidence-backed codebase intelligence"
  LANGUAGES CXX
)

include(GNUInstallDirs)
include(CTest)
include(CMakePackageConfigHelpers)

option(SHERPA_WARNINGS_AS_ERRORS "Treat Sherpa compiler warnings as errors" OFF)
option(SHERPA_BUILD_BENCHMARKS "Build manual Sherpa benchmarks" OFF)

set_property(
  DIRECTORY
  APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS
           "${CMAKE_CURRENT_SOURCE_DIR}/migrations/001_initial.sql"
           "${CMAKE_CURRENT_SOURCE_DIR}/migrations/002_syntax.sql"
           "${CMAKE_CURRENT_SOURCE_DIR}/migrations/003_relationships.sql"
           "${CMAKE_CURRENT_SOURCE_DIR}/migrations/004_call_sites.sql"
)

find_package(CLI11 CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(unofficial-tree-sitter CONFIG REQUIRED)
find_package(tree-sitter-c CONFIG REQUIRED)
find_package(tree-sitter-cpp CONFIG REQUIRED)

if(TARGET SQLite3::SQLite3)
  set(SHERPA_SQLITE_TARGET SQLite3::SQLite3)
elseif(TARGET SQLite::SQLite3)
  set(SHERPA_SQLITE_TARGET SQLite::SQLite3)
else()
  message(FATAL_ERROR "FindSQLite3 did not provide a supported imported target")
endif()

function(sherpa_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(SHERPA_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(
      ${target}
      PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    if(SHERPA_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/migrations/001_initial.sql" SHERPA_SCHEMA_SQL)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/migrations/002_syntax.sql" SHERPA_SYNTAX_SCHEMA_SQL)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/migrations/003_relationships.sql"
     SHERPA_RELATIONSHIP_SCHEMA_SQL)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/migrations/004_call_sites.sql"
     SHERPA_CALL_SITE_SCHEMA_SQL)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/storage/schema_sql.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/sherpa/storage/schema_sql.hpp"
  @ONLY
)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/sherpa/version.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/sherpa/version.hpp"
  @ONLY
)

add_library(
  sherpa_core
  STATIC
  src/api/client.cpp
  src/analysis/relationship_resolver.cpp
  src/application/call_query_service.cpp
  src/application/file_query_service.cpp
  src/application/graph_query_support.cpp
  src/application/graph_export_service.cpp
  src/application/impact_service.cpp
  src/application/index_service.cpp
  src/application/path_service.cpp
  src/application/query_errors.cpp
  src/application/symbol_query_service.cpp
  src/domain/call_query.cpp
  src/domain/file_query.cpp
  src/domain/graph_export.cpp
  src/domain/impact.cpp
  src/domain/path_query.cpp
  src/domain/relationship.cpp
  src/domain/symbol_query.cpp
  src/domain/syntax.cpp
  src/parsing/tree_sitter_frontend.cpp
  src/presentation/call_query_renderer.cpp
  src/presentation/file_query_renderer.cpp
  src/presentation/graph_export_renderer.cpp
  src/presentation/impact_renderer.cpp
  src/presentation/path_renderer.cpp
  src/presentation/query_error_renderer.cpp
  src/presentation/symbol_query_renderer.cpp
  src/scanner/repository_scanner.cpp
  src/storage/sqlite_database.cpp
  src/util/fingerprint.cpp
)
add_library(Sherpa::core ALIAS sherpa_core)
add_library(Sherpa::Sherpa ALIAS sherpa_core)
set_target_properties(sherpa_core PROPERTIES EXPORT_NAME Sherpa OUTPUT_NAME sherpa)

target_compile_features(sherpa_core PUBLIC cxx_std_20)
set_target_properties(sherpa_core PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(
  sherpa_core
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
  sherpa_core
  PUBLIC ${SHERPA_SQLITE_TARGET}
  PRIVATE
    unofficial::tree-sitter::tree-sitter
    tree-sitter::tree-sitter-c
    tree-sitter::tree-sitter-cpp
)
sherpa_enable_warnings(sherpa_core)

add_executable(sherpa src/cli/graph_export_command.cpp src/cli/main.cpp)
target_compile_features(sherpa PRIVATE cxx_std_20)
set_target_properties(sherpa PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(sherpa PRIVATE Sherpa::core CLI11::CLI11)
sherpa_enable_warnings(sherpa)

if(SHERPA_BUILD_BENCHMARKS)
  add_executable(sherpa_index_benchmark benchmarks/index_benchmark.cpp)
  target_compile_features(sherpa_index_benchmark PRIVATE cxx_std_20)
  set_target_properties(sherpa_index_benchmark PROPERTIES CXX_EXTENSIONS OFF)
  target_link_libraries(sherpa_index_benchmark PRIVATE Sherpa::core)
  sherpa_enable_warnings(sherpa_index_benchmark)
endif()

install(
  TARGETS sherpa_core
  EXPORT SherpaTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(TARGETS sherpa RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

set(
  SHERPA_PUBLIC_HEADERS
  include/sherpa/api/client.hpp
  include/sherpa/domain/call_query.hpp
  include/sherpa/domain/file_query.hpp
  include/sherpa/domain/graph_export.hpp
  include/sherpa/domain/impact.hpp
  include/sherpa/domain/path_query.hpp
  include/sherpa/domain/relationship.hpp
  include/sherpa/domain/symbol_query.hpp
  include/sherpa/domain/syntax.hpp
)
foreach(header IN LISTS SHERPA_PUBLIC_HEADERS)
  cmake_path(GET header PARENT_PATH header_directory)
  string(REPLACE "include/" "" install_directory "${header_directory}")
  install(FILES "${header}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${install_directory}")
endforeach()
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/sherpa/version.hpp"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sherpa"
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SherpaConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/SherpaConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Sherpa"
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/SherpaConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMinorVersion
)
install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SherpaConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SherpaConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Sherpa"
)
install(
  EXPORT SherpaTargets
  FILE SherpaTargets.cmake
  NAMESPACE Sherpa::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Sherpa"
)

if(BUILD_TESTING)
  find_package(Catch2 3 CONFIG REQUIRED)

  add_executable(
    sherpa_tests
    tests/unit/call_query_renderer_test.cpp
    tests/unit/file_query_renderer_test.cpp
    tests/unit/fingerprint_test.cpp
    tests/unit/impact_renderer_test.cpp
    tests/unit/path_renderer_test.cpp
    tests/unit/query_error_renderer_test.cpp
    tests/unit/relationship_resolver_test.cpp
    tests/unit/repository_scanner_test.cpp
    tests/unit/symbol_query_renderer_test.cpp
    tests/unit/tree_sitter_frontend_test.cpp
    tests/integration/call_query_service_test.cpp
    tests/integration/file_query_service_test.cpp
    tests/integration/graph_export_service_test.cpp
    tests/integration/impact_service_test.cpp
    tests/integration/index_service_test.cpp
    tests/integration/path_service_test.cpp
    tests/integration/public_api_test.cpp
    tests/integration/symbol_query_service_test.cpp
  )
  target_compile_features(sherpa_tests PRIVATE cxx_std_20)
  set_target_properties(sherpa_tests PROPERTIES CXX_EXTENSIONS OFF)
  target_compile_definitions(
    sherpa_tests PRIVATE SHERPA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_link_libraries(sherpa_tests PRIVATE Sherpa::core Catch2::Catch2WithMain)
  sherpa_enable_warnings(sherpa_tests)

  include(Catch)
  catch_discover_tests(sherpa_tests)

  add_test(NAME cli_help COMMAND sherpa --help)
  add_executable(sherpa_api_consumer tests/consumer/api_consumer.cpp)
  target_compile_features(sherpa_api_consumer PRIVATE cxx_std_20)
  set_target_properties(sherpa_api_consumer PROPERTIES CXX_EXTENSIONS OFF)
  target_link_libraries(sherpa_api_consumer PRIVATE Sherpa::Sherpa)
  sherpa_enable_warnings(sherpa_api_consumer)
  add_test(NAME api_build_tree_consumer COMMAND sherpa_api_consumer)

  get_property(sherpa_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  add_test(
    NAME api_installed_package_consumer
    COMMAND
      "${CMAKE_COMMAND}"
      -DSHERPA_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DSHERPA_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DSHERPA_GENERATOR=${CMAKE_GENERATOR}
      -DSHERPA_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
      -DSHERPA_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}
      -DSHERPA_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
      -DSHERPA_CONFIG=$<CONFIG>
      -DSHERPA_MULTI_CONFIG=${sherpa_is_multi_config}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/consumer/install_consumer.cmake
  )
  add_test(
    NAME cli_index_fixture
    COMMAND
      "${CMAKE_COMMAND}"
      -DSHERPA_EXECUTABLE=$<TARGET_FILE:sherpa>
      -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/basic_cpp
      -DDATABASE_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-index.sqlite
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/index_fixture.cmake
  )
  add_test(
    NAME cli_query
    COMMAND
      "${CMAKE_COMMAND}"
      -DSHERPA_EXECUTABLE=$<TARGET_FILE:sherpa>
      -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/relationships
      -DDATABASE_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-query-group.sqlite
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/query.cmake
  )
  add_test(
    NAME cli_call_queries
    COMMAND
      "${CMAKE_COMMAND}"
      -DSHERPA_EXECUTABLE=$<TARGET_FILE:sherpa>
      -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/relationships
      -DDATABASE_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-query.sqlite
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/call_queries.cmake
  )
  add_test(
    NAME cli_impact
    COMMAND
      "${CMAKE_COMMAND}"
      -DSHERPA_EXECUTABLE=$<TARGET_FILE:sherpa>
      -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/impact
      -DDATABASE_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-impact.sqlite
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/impact.cmake
  )
  add_test(
    NAME cli_path
    COMMAND
      "${CMAKE_COMMAND}"
      -DSHERPA_EXECUTABLE=$<TARGET_FILE:sherpa>
      -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/impact
      -DDATABASE_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-path.sqlite
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/path.cmake
  )
  add_test(
    NAME cli_export
    COMMAND
      "${CMAKE_COMMAND}"
      -DSHERPA_EXECUTABLE=$<TARGET_FILE:sherpa>
      -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/impact
      -DDATABASE_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-export.sqlite
      -DOUTPUT_PATH=${CMAKE_CURRENT_BINARY_DIR}/graph-export.json
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/export.cmake
  )
endif()
