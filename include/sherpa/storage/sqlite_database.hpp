#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "sherpa/domain/call_query.hpp"
#include "sherpa/domain/graph_snapshot.hpp"
#include "sherpa/domain/indexed_file.hpp"
#include "sherpa/domain/relationship.hpp"

struct sqlite3;

namespace sherpa {

enum class DatabaseOpenMode {
  kReadWriteCreate,
  kReadOnly,
};

class SqliteDatabase {
 public:
  explicit SqliteDatabase(const std::filesystem::path& path,
                          DatabaseOpenMode mode = DatabaseOpenMode::kReadWriteCreate);
  ~SqliteDatabase();

  SqliteDatabase(const SqliteDatabase&) = delete;
  SqliteDatabase& operator=(const SqliteDatabase&) = delete;
  SqliteDatabase(SqliteDatabase&&) = delete;
  SqliteDatabase& operator=(SqliteDatabase&&) = delete;

  void initialize_schema();
  void validate_schema() const;
  void replace_index(const std::filesystem::path& repository_path,
                     const std::vector<IndexedFile>& files,
                     const std::vector<RelationshipRecord>& relationships);
  [[nodiscard]] std::vector<IndexedFile> load_indexed_files(
      const std::filesystem::path& repository_path) const;
  [[nodiscard]] bool has_completed_index(const std::string& canonical_repository_path) const;
  [[nodiscard]] std::vector<CallQueryEntry> query_calls(std::int64_t symbol_id,
                                                        CallQueryDirection direction) const;
  [[nodiscard]] GraphSnapshot load_graph(const std::string& canonical_repository_path) const;

 private:
  void execute(const std::string& sql);
  [[nodiscard]] int schema_version() const;

  sqlite3* database_{};
};

}  // namespace sherpa
