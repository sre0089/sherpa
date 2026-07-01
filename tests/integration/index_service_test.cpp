#include "sherpa/application/index_service.hpp"

#include <sqlite3.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
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
  REQUIRE(first.added_files == 3);
  REQUIRE(first.parsed_files == 3);
  REQUIRE(first.unchanged_files == 0);
  REQUIRE(first.extracted_symbols == 4);
  REQUIRE(first.extracted_includes == 2);
  REQUIRE(first.diagnostics == 0);
  REQUIRE(first.relationships == 6);
  REQUIRE(first.resolved_relationships == 6);
  REQUIRE(second.indexed_files == 3);
  REQUIRE(second.added_files == 0);
  REQUIRE(second.modified_files == 0);
  REQUIRE(second.unchanged_files == 3);
  REQUIRE(second.deleted_files == 0);
  REQUIRE(second.parsed_files == 0);
  REQUIRE(second.extracted_symbols == 4);
  REQUIRE(second.relationships == 6);

  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM repositories") == 1);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM index_runs") == 1);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM files") == 3);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM files WHERE language = 'cpp'") == 3);
  REQUIRE(scalar(database, "SELECT version FROM schema_metadata") == 4);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM files WHERE parse_status = 'parsed'") == 3);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM symbols") == 4);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM include_directives") == 2);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM diagnostics") == 0);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM call_sites") == 1);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM relationships") == 6);
  REQUIRE(scalar(database,
                 "SELECT COUNT(*) FROM relationships "
                 "WHERE kind = 'calls' AND confidence = 'low'") == 1);
  sqlite3_close(database);
}

TEST_CASE("index service persists and reuses mixed C++ and Python analysis") {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path() / "python.sqlite";
  const auto fixture = std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "python";

  sherpa::IndexService service;
  const auto first = service.index({fixture, database_path});
  const auto second = service.index({fixture, database_path});

  REQUIRE(first.indexed_files == 6);
  REQUIRE(first.added_files == 6);
  REQUIRE(first.parsed_files == 6);
  REQUIRE(first.extracted_symbols == 8);
  REQUIRE(first.extracted_includes == 5);
  REQUIRE(first.relationships == 19);
  REQUIRE(first.resolved_relationships == 17);
  REQUIRE(first.unresolved_relationships == 2);
  REQUIRE(second.unchanged_files == 6);
  REQUIRE(second.parsed_files == 0);
  REQUIRE(second.relationships == first.relationships);

  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  CHECK(scalar(database, "SELECT COUNT(*) FROM files WHERE language = 'python'") == 5);
  CHECK(scalar(database, "SELECT COUNT(*) FROM files WHERE language = 'cpp'") == 1);
  CHECK(scalar(database,
               "SELECT COUNT(*) FROM relationships relationship "
               "JOIN symbols target ON target.id = relationship.target_symbol_id "
               "JOIN files target_file ON target_file.id = target.file_id "
               "WHERE relationship.kind = 'calls' AND target_file.language = 'python'") == 5);
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
  REQUIRE(scalar(database, "SELECT version FROM schema_metadata") == 4);
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
                       "DROP TABLE call_sites;"
                       "DROP TABLE relationship_candidates;"
                       "DROP TABLE relationships;"
                       "ALTER TABLE index_runs DROP COLUMN analysis_cache_version;"
                       "UPDATE schema_metadata SET version = 2;",
                       nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(database);

  const auto result = service.index({fixture, database_path});
  REQUIRE(result.relationships == 6);

  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database, "SELECT version FROM schema_metadata") == 4);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM relationships") == 6);
  sqlite3_close(database);
}

TEST_CASE("index service migrates schema version three and invalidates incomplete caches") {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "basic_cpp";

  sherpa::IndexService service;
  static_cast<void>(service.index({fixture, database_path}));

  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(sqlite3_exec(database,
                       "DROP TABLE call_sites;"
                       "ALTER TABLE index_runs DROP COLUMN analysis_cache_version;"
                       "UPDATE schema_metadata SET version = 3;",
                       nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(database);

  const auto result = service.index({fixture, database_path});
  REQUIRE(result.parsed_files == 3);
  REQUIRE(result.unchanged_files == 0);

  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database, "SELECT version FROM schema_metadata") == 4);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM call_sites") == 1);
  sqlite3_close(database);
}

TEST_CASE("index service reparses only added and modified files and removes deleted files") {
  TemporaryDirectory temporary;
  const auto source = std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "basic_cpp";
  const auto repository = temporary.path() / "repository";
  const auto database_path = temporary.path() / "incremental.sqlite";
  std::filesystem::copy(source, repository, std::filesystem::copy_options::recursive);

  sherpa::IndexService service;
  const auto initial = service.index({repository, database_path});
  REQUIRE(initial.added_files == 3);
  REQUIRE(initial.parsed_files == 3);

  {
    std::ofstream modified(repository / "src" / "main.cpp", std::ios::app);
    REQUIRE(modified.good());
    modified << "\nint incremental_marker() { return 7; }\n";
  }
  {
    std::ofstream added(repository / "src" / "extra.cpp");
    REQUIRE(added.good());
    added << "int extra_definition() { return 9; }\n";
  }
  REQUIRE(std::filesystem::remove(repository / "src" / "calculator.cpp"));

  const auto incremental = service.index({repository, database_path});
  REQUIRE(incremental.indexed_files == 3);
  REQUIRE(incremental.added_files == 1);
  REQUIRE(incremental.modified_files == 1);
  REQUIRE(incremental.unchanged_files == 1);
  REQUIRE(incremental.deleted_files == 1);
  REQUIRE(incremental.parsed_files == 2);
  REQUIRE(incremental.extracted_symbols == 5);

  const auto unchanged = service.index({repository, database_path});
  REQUIRE(unchanged.unchanged_files == 3);
  REQUIRE(unchanged.parsed_files == 0);

  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM files") == 3);
  REQUIRE(scalar(database,
                 "SELECT COUNT(*) FROM files WHERE relative_path = 'src/calculator.cpp'") == 0);
  sqlite3_close(database);
}

TEST_CASE("failed incremental persistence preserves the prior active snapshot") {
  TemporaryDirectory temporary;
  const auto source = std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "basic_cpp";
  const auto repository = temporary.path() / "repository";
  const auto database_path = temporary.path() / "atomic.sqlite";
  std::filesystem::copy(source, repository, std::filesystem::copy_options::recursive);

  sherpa::IndexService service;
  static_cast<void>(service.index({repository, database_path}));

  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(sqlite3_exec(database,
                       "CREATE TRIGGER force_file_insert_failure "
                       "BEFORE INSERT ON files BEGIN "
                       "SELECT RAISE(ABORT, 'forced persistence failure'); END;",
                       nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(database);

  {
    std::ofstream modified(repository / "src" / "main.cpp", std::ios::app);
    REQUIRE(modified.good());
    modified << "\nint should_not_publish() { return 1; }\n";
  }

  REQUIRE_THROWS_WITH(service.index({repository, database_path}), "forced persistence failure");

  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM index_runs") == 1);
  REQUIRE(scalar(database, "SELECT COUNT(*) FROM files") == 3);
  REQUIRE(scalar(database,
                 "SELECT COUNT(*) FROM symbols WHERE qualified_name = 'should_not_publish'") == 0);
  sqlite3_close(database);
}

TEST_CASE("incremental indexing rebuilds relationships from reused call sites") {
  TemporaryDirectory temporary;
  const auto repository = temporary.path() / "repository";
  const auto database_path = temporary.path() / "relationships.sqlite";
  std::filesystem::create_directories(repository);
  {
    std::ofstream caller(repository / "caller.cpp");
    REQUIRE(caller.good());
    caller << "int target();\nint run() { return target(); }\n";
  }
  {
    std::ofstream first_target(repository / "first.cpp");
    REQUIRE(first_target.good());
    first_target << "int target() { return 1; }\n";
  }
  {
    std::ofstream second_target(repository / "second.cpp");
    REQUIRE(second_target.good());
    second_target << "int target() { return 2; }\n";
  }

  sherpa::IndexService service;
  static_cast<void>(service.index({repository, database_path}));

  sqlite3* database = nullptr;
  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database,
                 "SELECT COUNT(*) FROM relationships "
                 "WHERE kind = 'calls' AND resolution = 'ambiguous'") == 1);
  sqlite3_close(database);

  REQUIRE(std::filesystem::remove(repository / "second.cpp"));
  const auto incremental = service.index({repository, database_path});
  REQUIRE(incremental.parsed_files == 0);
  REQUIRE(incremental.unchanged_files == 2);
  REQUIRE(incremental.deleted_files == 1);

  REQUIRE(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK);
  REQUIRE(scalar(database,
                 "SELECT COUNT(*) FROM relationships "
                 "WHERE kind = 'calls' AND resolution = 'resolved'") == 1);
  REQUIRE(scalar(database,
                 "SELECT COUNT(*) FROM relationships "
                 "WHERE kind = 'calls' AND resolution = 'ambiguous'") == 0);
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
