cmake_minimum_required(VERSION 3.13)
project(tree-sitter-c VERSION 0.24.2 LANGUAGES C)

include(GNUInstallDirs)

add_library(tree-sitter-c src/parser.c)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/scanner.c")
  target_sources(tree-sitter-c PRIVATE src/scanner.c)
endif()
target_include_directories(
  tree-sitter-c
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bindings/c>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
set_target_properties(
  tree-sitter-c
  PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

install(
  TARGETS tree-sitter-c
  EXPORT tree-sitter-c-config
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bindings/c/tree_sitter"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING PATTERN "*.h"
)
install(
  EXPORT tree-sitter-c-config
  FILE tree-sitter-c-config.cmake
  NAMESPACE tree-sitter::
  DESTINATION "share/tree-sitter-c"
)
