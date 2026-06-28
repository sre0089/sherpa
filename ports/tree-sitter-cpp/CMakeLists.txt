cmake_minimum_required(VERSION 3.13)
project(tree-sitter-cpp VERSION 0.23.4 LANGUAGES C)

include(GNUInstallDirs)

add_library(tree-sitter-cpp src/parser.c src/scanner.c)
target_include_directories(
  tree-sitter-cpp
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bindings/c>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
set_target_properties(
  tree-sitter-cpp
  PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

install(
  TARGETS tree-sitter-cpp
  EXPORT tree-sitter-cpp-config
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/bindings/c/tree-sitter-cpp.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tree_sitter"
)
install(
  EXPORT tree-sitter-cpp-config
  FILE tree-sitter-cpp-config.cmake
  NAMESPACE tree-sitter::
  DESTINATION "share/tree-sitter-cpp"
)
