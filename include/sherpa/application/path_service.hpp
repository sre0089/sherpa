#pragma once

#include <filesystem>
#include <string>

#include "sherpa/application/query_errors.hpp"
#include "sherpa/domain/path_query.hpp"

namespace sherpa {

struct PathOptions {
  std::string source;
  std::string target;
  std::string source_signature;
  std::string source_file_path;
  std::string target_signature;
  std::string target_file_path;
  std::filesystem::path repository_path{"."};
  std::filesystem::path database_path;
};

class PathService {
 public:
  [[nodiscard]] PathResult find(const PathOptions& options) const;
};

}  // namespace sherpa
