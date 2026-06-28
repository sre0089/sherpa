vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO tree-sitter/tree-sitter-cpp
  REF "v${VERSION}"
  SHA512 baebacf06ea1527132c641b4e2a2e997c501a63708d7afdb5d9456de519dbd652f25aee03a7b4112ef9a683fa176aaaf96d272de286223773a5d6cdf01605a2e
  HEAD_REF master
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME tree-sitter-cpp)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
