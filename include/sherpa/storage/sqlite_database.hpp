#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "sherpa/domain/indexed_file.hpp"

struct sqlite3;

namespace sherpa {

class SqliteDatabase {
 public:
  explicit SqliteDatabase(const std::filesystem::path& path);
  ~SqliteDatabase();

  SqliteDatabase(const SqliteDatabase&) = delete;
  SqliteDatabase& operator=(const SqliteDatabase&) = delete;
  SqliteDatabase(SqliteDatabase&&) = delete;
  SqliteDatabase& operator=(SqliteDatabase&&) = delete;

  void initialize_schema();
  void replace_index(const std::filesystem::path& repository_path,
                     const std::vector<IndexedFile>& files);

 private:
  void execute(const std::string& sql);
  [[nodiscard]] int schema_version() const;

  sqlite3* database_{};
};

}  // namespace sherpa
