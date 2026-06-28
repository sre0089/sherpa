vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO tree-sitter/tree-sitter-c
  REF "v${VERSION}"
  SHA512 daa56178adfc4cc7931fb2810367e531113e07ebadcf51cdf7645281627cb86c028c5bb32a1306d294e79d314ef19ce185dfd8786732d5c81a9f3c870396249c
  HEAD_REF master
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME tree-sitter-c)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
