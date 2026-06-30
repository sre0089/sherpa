#include "graph_export_command.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "sherpa/api/client.hpp"
#include "sherpa/presentation/graph_export_renderer.hpp"

namespace sherpa::cli {
namespace {

std::filesystem::path temporary_path_for(const std::filesystem::path& output_path) {
  const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
  return output_path.parent_path() /
         (output_path.filename().string() + ".sherpa-tmp-" + std::to_string(suffix));
}

}  // namespace

GraphExportCommandResult export_graph(const GraphExportCommandOptions& options) {
  if (options.output_path.empty()) {
    throw ExportOutputError("output path is required");
  }

  const auto output_path = std::filesystem::absolute(options.output_path);
  const auto parent_path = output_path.parent_path();
  if (!std::filesystem::is_directory(parent_path)) {
    throw ExportOutputError("output directory does not exist: " + parent_path.string());
  }
  if (std::filesystem::exists(output_path)) {
    if (!options.force) {
      throw ExportOutputError("output file already exists; use --force to replace it: " +
                              output_path.string());
    }
    if (!std::filesystem::is_regular_file(output_path)) {
      throw ExportOutputError("output path is not a regular file: " + output_path.string());
    }
  }

  const auto graph = api::Client{}.graph({
      .path = options.repository_path,
      .database_path = options.database_path,
  });
  const auto temporary_path = temporary_path_for(output_path);
  try {
    std::ofstream output(temporary_path, std::ios::binary | std::ios::trunc);
    if (!output) {
      throw ExportOutputError("could not create temporary export file: " + temporary_path.string());
    }
    write_graph_export_json(output, graph);
    output.flush();
    if (!output) {
      throw ExportOutputError("failed while writing graph export: " + output_path.string());
    }
    output.close();

    std::error_code error;
    if (options.force && std::filesystem::exists(output_path)) {
      std::filesystem::remove(output_path, error);
      if (error) {
        throw ExportOutputError("could not replace output file: " + error.message());
      }
    }
    std::filesystem::rename(temporary_path, output_path, error);
    if (error) {
      throw ExportOutputError("could not finalize graph export: " + error.message());
    }
  } catch (...) {
    std::error_code cleanup_error;
    std::filesystem::remove(temporary_path, cleanup_error);
    throw;
  }

  return GraphExportCommandResult{
      .output_path = output_path,
      .nodes = graph.nodes.size(),
      .edges = graph.edges.size(),
  };
}

}  // namespace sherpa::cli
