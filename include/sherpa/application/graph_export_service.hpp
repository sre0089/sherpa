#pragma once

#include <filesystem>

#include "sherpa/application/query_errors.hpp"
#include "sherpa/domain/graph_export.hpp"

namespace sherpa {

struct GraphExportOptions {
  std::filesystem::path repository_path{"."};
  std::filesystem::path database_path;
};

class GraphExportService {
 public:
  [[nodiscard]] GraphExport build(const GraphExportOptions& options) const;
};

}  // namespace sherpa
