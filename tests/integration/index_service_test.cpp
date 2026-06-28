#include "sherpa/application/index_service.hpp"

#include <sqlite3.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <chrono>
#include <filesystem>
#include <stdexcept>

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
  REQUIRE(first.extracted_symbols == 4);
  REQUIRE(first.extracted_includes == 2);
  REQUIRE(first.diagnostics == 0);
  REQUIRE(first.relationships == 6);
  REQUIRE(first.resolved_relationships == 6);
  REQUIRE(second.indexed_files == 3);
  REQUIRE(second.extracted_symbols == 4);
  REQUIRE(second.relationships == 6);

  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM repositories") == 1);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM index_runs") == 1);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM files") == 3);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM files WHERE language = 'cpp'") == 3);
  REQUIRE(scalar(database, "SELECT version FROM schema_metadata") == 3);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM files WHERE parse_status = 'parsed'") == 3);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM symbols") == 4);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM include_directives") == 2);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM diagnostics") == 0);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM relationships") == 6);
  REQUIRE(scalar(database,
                 "SELECT COUNT(*) FROM relationships "
                 "WHERE kind = 'calls' AND confidence = 'low'") == 1);
  sqlite3_close(database);
}

TEST_CASE("index service rejects an incompatible database schema") {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(sqlite3_exec(database,
                       "CREATE TABLE schema_metadata(version INTEGER NOT NULL);"
                       "INSERT INTO schema_metadata VALUES (99);",
                       nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(database);

  const auto fixture =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "basic_cpp";
  REQUIRE_THROWS_WITH(sherpa::IndexService{}.index({fixture, database_path}),
                      "unsupported database schema version: 99");
}

TEST_CASE("index service migrates a schema version one database") {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(
      sqlite3_exec(
          database,
          "PRAGMA foreign_keys = ON;"
          "CREATE TABLE schema_metadata(version INTEGER NOT NULL);"
          "INSERT INTO schema_metadata VALUES (1);"
          "CREATE TABLE repositories("
          "id INTEGER PRIMARY KEY, canonical_path TEXT NOT NULL UNIQUE, active_run_id INTEGER);"
          "CREATE TABLE index_runs("
          "id INTEGER PRIMARY KEY, repository_id INTEGER NOT NULL REFERENCES repositories(id) "
          "ON DELETE CASCADE, started_at TEXT NOT NULL, completed_at TEXT, status TEXT NOT NULL);"
          "CREATE TABLE files("
          "id INTEGER PRIMARY KEY, run_id INTEGER NOT NULL REFERENCES index_runs(id) "
          "ON DELETE CASCADE, relative_path TEXT NOT NULL, language TEXT NOT NULL, "
          "content_fingerprint TEXT NOT NULL, size_bytes INTEGER NOT NULL, "
          "UNIQUE(run_id, relative_path));",
          nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(database);

  const auto fixture =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "basic_cpp";
  const auto result = sherpa::IndexService{}.index({fixture, database_path});
  REQUIRE(result.extracted_symbols == 4);

  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database, "SELECT version FROM schema_metadata") == 3);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM symbols") == 4);
  sqlite3_close(database);
}

TEST_CASE("index service persists syntax results and diagnostics") {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path() / "syntax.sqlite";
  const auto fixture =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "syntax_cpp";

  const auto result = sherpa::IndexService{}.index({fixture, database_path});
  REQUIRE(result.indexed_files == 4);
  REQUIRE(result.extracted_symbols == 13);
  REQUIRE(result.extracted_includes == 3);
  REQUIRE(result.diagnostics == 1);

  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM symbols") == 13);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM include_directives") == 3);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM diagnostics") == 1);
  REQUIRE(scalar(database,
                 "SELECT COUNT(*) FROM files WHERE parse_status = 'parsed_with_errors'") == 1);
  REQUIRE(scalar(database,
                 "SELECT COUNT(*) FROM symbols WHERE qualified_name = 'demo::Widget::compute'") ==
          2);
  sqlite3_close(database);
}

TEST_CASE("index service migrates a schema version two database") {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "basic_cpp";

  sherpa::IndexService service;
  static_cast<void>(service.index({fixture, database_path}));

  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(sqlite3_exec(database,
                       "DROP TABLE relationship_candidates;"
                       "DROP TABLE relationships;"
                       "UPDATE schema_metadata SET version = 2;",
                       nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(database);

  const auto result = service.index({fixture, database_path});
  REQUIRE(result.relationships == 6);

  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database, "SELECT version FROM schema_metadata") == 3);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM relationships") == 6);
  sqlite3_close(database);
}

TEST_CASE("index service persists relationship evidence and candidates") {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path() / "relationships.sqlite";
  const auto fixture =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "relationships";

  const auto result = sherpa::IndexService{}.index({fixture, database_path});
  REQUIRE(result.indexed_files == 5);
  REQUIRE(result.relationships == 11);
  REQUIRE(result.resolved_relationships == 7);
  REQUIRE(result.ambiguous_relationships == 2);
  REQUIRE(result.unresolved_relationships == 2);

  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM relationships") == 11);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM relationships WHERE resolution = 'ambiguous'") ==
          2);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM relationships WHERE resolution = 'unresolved'") ==
          2);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM relationship_candidates") == 4);
  REQUIRE(scalar(database,
                 "SELECT COUNT(*) FROM relationships "
                 "WHERE kind = 'calls' AND source_symbol_id IS NOT NULL") == 3);
  sqlite3_close(database);
}
