cmake_minimum_required(VERSION 3.13)
project(tree-sitter-python VERSION 0.25.0 LANGUAGES C)

include(GNUInstallDirs)

add_library(tree-sitter-python src/parser.c src/scanner.c)
target_include_directories(
  tree-sitter-python
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bindings/c>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
set_target_properties(
  tree-sitter-python
  PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

install(
  TARGETS tree-sitter-python
  EXPORT tree-sitter-python-config
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
  EXPORT tree-sitter-python-config
  FILE tree-sitter-python-config.cmake
  NAMESPACE tree-sitter::
  DESTINATION "share/tree-sitter-python"
)
