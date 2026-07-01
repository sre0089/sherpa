vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO tree-sitter/tree-sitter-python
  REF "v${VERSION}"
  SHA512 9491de1eb1cb3962aa4894fad5b6bcc05dc3ddeef902a1982d0ac6d7f3baa78fc52c2a21d1e953b1859ed7a104c8182dbd8bd91ced41e63cc2a647f779d86456
  HEAD_REF master
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME tree-sitter-python)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
