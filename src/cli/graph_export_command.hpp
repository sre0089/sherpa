#pragma once

#include <cstddef>
#include <filesystem>
#include <stdexcept>

namespace sherpa::cli {

struct GraphExportCommandOptions {
  std::filesystem::path output_path;
  std::filesystem::path repository_path{"."};
  std::filesystem::path database_path;
  bool force{};
};

struct GraphExportCommandResult {
  std::filesystem::path output_path;
  std::size_t nodes{};
  std::size_t edges{};
};

class ExportOutputError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

[[nodiscard]] GraphExportCommandResult export_graph(const GraphExportCommandOptions& options);

}  // namespace sherpa::cli
