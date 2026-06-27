#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "sherpa/domain/file_record.hpp"

namespace sherpa {

struct ScanResult {
  std::vector<FileRecord> files;
  std::vector<std::string> warnings;
};

class RepositoryScanner {
 public:
  [[nodiscard]] ScanResult scan(const std::filesystem::path& repository_path) const;
  [[nodiscard]] static bool is_supported_source(const std::filesystem::path& path);
};

}  // namespace sherpa
