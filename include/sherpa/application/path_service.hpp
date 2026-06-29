#pragma once

#include <filesystem>
#include <string>

#include "sherpa/application/query_errors.hpp"
#include "sherpa/domain/path_query.hpp"

namespace sherpa {

struct PathOptions {
  std::string source;
  std::string target;
  std::filesystem::path repository_path{"."};
  std::filesystem::path database_path;
};

class PathService {
 public:
  [[nodiscard]] PathResult find(const PathOptions& options) const;
};

}  // namespace sherpa
