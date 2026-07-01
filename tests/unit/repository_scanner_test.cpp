#include "sherpa/scanner/repository_scanner.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#ifndef SHERPA_SOURCE_DIR
#define SHERPA_SOURCE_DIR "."
#endif

TEST_CASE("scanner recognizes supported C C++ and Python extensions") {
  REQUIRE(sherpa::RepositoryScanner::is_supported_source("source.cpp"));
  REQUIRE(sherpa::RepositoryScanner::is_supported_source("HEADER.HPP"));
  REQUIRE(sherpa::RepositoryScanner::is_supported_source("module.py"));
  REQUIRE(sherpa::RepositoryScanner::is_supported_source("types.PYI"));
  REQUIRE_FALSE(sherpa::RepositoryScanner::is_supported_source("README.md"));
}

TEST_CASE("scanner returns fixture files in deterministic order") {
  const auto fixture =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "basic_cpp";
  const auto result = sherpa::RepositoryScanner{}.scan(fixture);

  REQUIRE(result.files.size() == 3);
  REQUIRE(result.files[0].relative_path == "include/calculator.hpp");
  REQUIRE(result.files[1].relative_path == "src/calculator.cpp");
  REQUIRE(result.files[2].relative_path == "src/main.cpp");
}

TEST_CASE("scanner assigns Python language deterministically") {
  const auto fixture = std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "python";
  const auto result = sherpa::RepositoryScanner{}.scan(fixture);

  REQUIRE(result.files.size() == 6);
  CHECK(result.files[0].relative_path == "app/__init__.py");
  CHECK(result.files[0].language == "python");
  CHECK(result.files[4].relative_path == "app/utils.py");
  CHECK(result.files[4].language == "python");
  CHECK(result.files[5].relative_path == "native.cpp");
  CHECK(result.files[5].language == "cpp");
}
