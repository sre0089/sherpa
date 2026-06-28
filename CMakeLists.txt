cmake_minimum_required(VERSION 3.25)

project(
  sherpa
  VERSION 0.1.0
  DESCRIPTION "Evidence-backed codebase intelligence"
  LANGUAGES CXX
)

include(GNUInstallDirs)
include(CTest)

option(SHERPA_WARNINGS_AS_ERRORS "Treat Sherpa compiler warnings as errors" OFF)

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
  src/application/index_service.cpp
  src/domain/syntax.cpp
  src/parsing/tree_sitter_frontend.cpp
  src/scanner/repository_scanner.cpp
  src/storage/sqlite_database.cpp
  src/util/fingerprint.cpp
)
add_library(Sherpa::core ALIAS sherpa_core)

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

add_executable(sherpa src/cli/main.cpp)
target_compile_features(sherpa PRIVATE cxx_std_20)
set_target_properties(sherpa PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(sherpa PRIVATE Sherpa::core CLI11::CLI11)
sherpa_enable_warnings(sherpa)

install(TARGETS sherpa RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(BUILD_TESTING)
  find_package(Catch2 3 CONFIG REQUIRED)

  add_executable(
    sherpa_tests
    tests/unit/fingerprint_test.cpp
    tests/unit/repository_scanner_test.cpp
    tests/unit/tree_sitter_frontend_test.cpp
    tests/integration/index_service_test.cpp
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
  add_test(
    NAME cli_index_fixture
    COMMAND
      "${CMAKE_COMMAND}"
      -DSHERPA_EXECUTABLE=$<TARGET_FILE:sherpa>
      -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/basic_cpp
      -DDATABASE_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-index.sqlite
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/index_fixture.cmake
  )
endif()
