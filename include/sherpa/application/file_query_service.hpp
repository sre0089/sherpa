#pragma once

#include <filesystem>
#include <string>

#include "sherpa/application/query_errors.hpp"
#include "sherpa/domain/file_query.hpp"

namespace sherpa {

struct FileQueryOptions {
  std::string path;
  std::filesystem::path repository_path{"."};
  std::filesystem::path database_path;
};

class FileQueryService {
 public:
  [[nodiscard]] FileQueryResult query(const FileQueryOptions& options) const;
};

}  // namespace sherpa
