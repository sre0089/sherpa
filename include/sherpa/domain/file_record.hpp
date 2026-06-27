#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace sherpa {

struct FileRecord {
  std::filesystem::path absolute_path;
  std::string relative_path;
  std::string language;
  std::string content_fingerprint;
  std::uintmax_t size_bytes{};
};

}  // namespace sherpa
