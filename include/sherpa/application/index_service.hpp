#pragma once

#include <cstddef>
#include <cstdint>
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
  std::size_t added_files{};
  std::size_t modified_files{};
  std::size_t unchanged_files{};
  std::size_t deleted_files{};
  std::size_t parsed_files{};
  std::size_t extracted_symbols{};
  std::size_t extracted_includes{};
  std::size_t diagnostics{};
  std::size_t relationships{};
  std::size_t resolved_relationships{};
  std::size_t ambiguous_relationships{};
  std::size_t unresolved_relationships{};
  std::uint64_t scan_milliseconds{};
  std::uint64_t cache_load_milliseconds{};
  std::uint64_t parse_milliseconds{};
  std::uint64_t relationship_milliseconds{};
  std::uint64_t persistence_milliseconds{};
  std::uint64_t total_milliseconds{};
  std::vector<std::string> warnings;
};

class IndexService {
 public:
  [[nodiscard]] IndexResult index(const IndexOptions& options) const;
  [[nodiscard]] static std::filesystem::path default_database_path(
      const std::filesystem::path& repository_path);
};

}  // namespace sherpa
