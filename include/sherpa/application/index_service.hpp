#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace sherpa {

struct IndexOptions {
  std::filesystem::path repository_path;
  std::filesystem::path database_path;
};

struct IndexResult {
  std::filesystem::path repository_path;
  std::filesystem::path database_path;
  std::size_t indexed_files{};
  std::size_t extracted_symbols{};
  std::size_t extracted_includes{};
  std::size_t diagnostics{};
  std::vector<std::string> warnings;
};

class IndexService {
 public:
  [[nodiscard]] IndexResult index(const IndexOptions& options) const;
  [[nodiscard]] static std::filesystem::path default_database_path(
      const std::filesystem::path& repository_path);
};

}  // namespace sherpa
