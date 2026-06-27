#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <stdexcept>

#include "sherpa/application/index_service.hpp"

#ifndef SHERPA_SOURCE_DIR
#define SHERPA_SOURCE_DIR "."
#endif

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() / ("sherpa-test-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

int scalar(sqlite3* database, const char* sql) {
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(database));
  }
  const int status = sqlite3_step(statement);
  const int value = status == SQLITE_ROW ? sqlite3_column_int(statement, 0) : -1;
  sqlite3_finalize(statement);
  return value;
}

}  // namespace

TEST_CASE("index service stores one active deterministic file index") {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "basic_cpp";

  sherpa::IndexService service;
  const auto first = service.index({fixture, database_path});
  const auto second = service.index({fixture, database_path});

  REQUIRE(first.indexed_files == 3);
  REQUIRE(second.indexed_files == 3);

  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM repositories") == 1);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM index_runs") == 1);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM files") == 3);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM files WHERE language = 'cpp'") == 3);
  sqlite3_close(database);
}

TEST_CASE("index service rejects an incompatible database schema") {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(sqlite3_exec(database, "CREATE TABLE schema_metadata(version INTEGER NOT NULL);"
                                "INSERT INTO schema_metadata VALUES (99);",
                       nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(database);

  const auto fixture =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "basic_cpp";
  REQUIRE_THROWS_WITH(sherpa::IndexService{}.index({fixture, database_path}),
                      "unsupported database schema version: 99");
}
