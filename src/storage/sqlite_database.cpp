#include "sherpa/storage/sqlite_database.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <stdexcept>
#include <string>

#include "sherpa/storage/schema_sql.hpp"

namespace sherpa {
namespace {

class Statement {
 public:
  Statement(sqlite3* database, const char* sql) {
    if (sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(database));
    }
  }

  ~Statement() { sqlite3_finalize(statement_); }

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  sqlite3_stmt* get() const { return statement_; }

 private:
  sqlite3_stmt* statement_{};
};

void check(sqlite3* database, int status) {
  if (status != SQLITE_OK && status != SQLITE_DONE && status != SQLITE_ROW) {
    throw std::runtime_error(sqlite3_errmsg(database));
  }
}

void bind_text(sqlite3* database, sqlite3_stmt* statement, int index, const std::string& value) {
  check(database,
        sqlite3_bind_text(statement, index, value.c_str(), static_cast<int>(value.size()),
                          SQLITE_TRANSIENT));
}

std::int64_t repository_id(sqlite3* database, const std::string& canonical_path) {
  Statement statement(database, "SELECT id FROM repositories WHERE canonical_path = ?1");
  bind_text(database, statement.get(), 1, canonical_path);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    throw std::runtime_error("repository row was not created");
  }
  return sqlite3_column_int64(statement.get(), 0);
}

}  // namespace

SqliteDatabase::SqliteDatabase(const std::filesystem::path& path) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  const auto path_string = path.string();
  if (sqlite3_open_v2(path_string.c_str(), &database_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                      nullptr) != SQLITE_OK) {
    const std::string message = database_ == nullptr ? "could not open SQLite database"
                                                     : sqlite3_errmsg(database_);
    sqlite3_close(database_);
    database_ = nullptr;
    throw std::runtime_error(message);
  }
  sqlite3_busy_timeout(database_, 5000);
}

SqliteDatabase::~SqliteDatabase() {
  if (database_ != nullptr) {
    sqlite3_close(database_);
  }
}

void SqliteDatabase::execute(const std::string& sql) {
  char* error_message = nullptr;
  const int status = sqlite3_exec(database_, sql.c_str(), nullptr, nullptr, &error_message);
  if (status != SQLITE_OK) {
    const std::string message = error_message == nullptr ? sqlite3_errmsg(database_) : error_message;
    sqlite3_free(error_message);
    throw std::runtime_error(message);
  }
}

int SqliteDatabase::schema_version() const {
  Statement statement(database_, "SELECT version FROM schema_metadata");
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    throw std::runtime_error("database schema version is missing");
  }
  return sqlite3_column_int(statement.get(), 0);
}

void SqliteDatabase::initialize_schema() {
  execute(kInitialSchema);
  constexpr int kSupportedSchemaVersion = 1;
  const int version = schema_version();
  if (version != kSupportedSchemaVersion) {
    throw std::runtime_error("unsupported database schema version: " + std::to_string(version));
  }
}

void SqliteDatabase::replace_file_index(const std::filesystem::path& repository_path,
                                        const std::vector<FileRecord>& files) {
  execute("BEGIN IMMEDIATE");
  try {
    const auto canonical_path = std::filesystem::canonical(repository_path).generic_string();

    {
      Statement statement(
          database_,
          "INSERT INTO repositories(canonical_path) VALUES (?1) "
          "ON CONFLICT(canonical_path) DO NOTHING");
      bind_text(database_, statement.get(), 1, canonical_path);
      check(database_, sqlite3_step(statement.get()));
    }

    const auto repo_id = repository_id(database_, canonical_path);
    std::int64_t run_id = 0;
    {
      Statement statement(
          database_,
          "INSERT INTO index_runs(repository_id, started_at, status) "
          "VALUES (?1, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), 'running')");
      check(database_, sqlite3_bind_int64(statement.get(), 1, repo_id));
      check(database_, sqlite3_step(statement.get()));
      run_id = sqlite3_last_insert_rowid(database_);
    }

    Statement insert_file(
        database_,
        "INSERT INTO files(run_id, relative_path, language, content_fingerprint, size_bytes) "
        "VALUES (?1, ?2, ?3, ?4, ?5)");
    for (const auto& file : files) {
      sqlite3_reset(insert_file.get());
      sqlite3_clear_bindings(insert_file.get());
      check(database_, sqlite3_bind_int64(insert_file.get(), 1, run_id));
      bind_text(database_, insert_file.get(), 2, file.relative_path);
      bind_text(database_, insert_file.get(), 3, file.language);
      bind_text(database_, insert_file.get(), 4, file.content_fingerprint);
      check(database_, sqlite3_bind_int64(insert_file.get(), 5,
                                          static_cast<sqlite3_int64>(file.size_bytes)));
      check(database_, sqlite3_step(insert_file.get()));
    }

    {
      Statement statement(
          database_,
          "UPDATE index_runs SET completed_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), "
          "status = 'completed' WHERE id = ?1");
      check(database_, sqlite3_bind_int64(statement.get(), 1, run_id));
      check(database_, sqlite3_step(statement.get()));
    }
    {
      Statement statement(database_, "UPDATE repositories SET active_run_id = ?1 WHERE id = ?2");
      check(database_, sqlite3_bind_int64(statement.get(), 1, run_id));
      check(database_, sqlite3_bind_int64(statement.get(), 2, repo_id));
      check(database_, sqlite3_step(statement.get()));
    }
    {
      Statement statement(
          database_, "DELETE FROM index_runs WHERE repository_id = ?1 AND id <> ?2");
      check(database_, sqlite3_bind_int64(statement.get(), 1, repo_id));
      check(database_, sqlite3_bind_int64(statement.get(), 2, run_id));
      check(database_, sqlite3_step(statement.get()));
    }

    execute("COMMIT");
  } catch (...) {
    try {
      execute("ROLLBACK");
    } catch (...) {
    }
    throw;
  }
}

}  // namespace sherpa
