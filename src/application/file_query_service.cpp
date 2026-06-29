#include "sherpa/application/file_query_service.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <tuple>

#include "graph_query_support.hpp"

namespace sherpa {
namespace {

bool include_less(const FileIncludeRecord& left, const FileIncludeRecord& right) {
  return std::tuple(left.source_path, left.target_path.value_or(""), left.target_text,
                    left.evidence.start_line, left.evidence.start_column, left.resolution,
                    left.confidence, left.provenance) <
         std::tuple(right.source_path, right.target_path.value_or(""), right.target_text,
                    right.evidence.start_line, right.evidence.start_column, right.resolution,
                    right.confidence, right.provenance);
}

FileIncludeRecord to_include_record(const GraphIncludeEdge& edge,
                                    const std::map<GraphNodeId, std::string>& file_paths) {
  FileIncludeRecord record{
      .source_path = file_paths.at(edge.source_file_id),
      .target_path = edge.target_file_id ? std::optional<std::string>{file_paths.at(*edge.target_file_id)}
                                         : std::nullopt,
      .target_text = edge.target_text,
      .resolution = edge.resolution,
      .confidence = edge.confidence,
      .provenance = edge.provenance,
      .evidence = edge.evidence,
      .candidates = {},
  };
  record.candidates.reserve(edge.candidates.size());
  for (const auto& candidate : edge.candidates) {
    record.candidates.push_back(FileQueryCandidate{
        .path = file_paths.at(candidate.node_id),
        .reason = candidate.reason,
        .rank = candidate.rank,
    });
  }
  return record;
}

}  // namespace

FileQueryResult FileQueryService::query(const FileQueryOptions& options) const {
  if (options.path.empty()) {
    throw std::invalid_argument("file path is required");
  }

  const auto loaded = load_query_graph(options.repository_path, options.database_path);
  const auto normalized = normalize_repository_relative_path(options.path, loaded.repository_path);

  const auto file = std::ranges::find(loaded.graph.files, normalized,
                                      [](const GraphFileNode& node) { return node.path; });
  if (file == loaded.graph.files.end()) {
    throw FileNotFoundError("file not found: " + options.path);
  }

  std::map<GraphNodeId, std::string> file_paths;
  for (const auto& node : loaded.graph.files) {
    file_paths.emplace(node.id, node.path);
  }

  FileQueryResult result{
      .path = file->path,
      .definitions = {},
      .incoming_includes = {},
      .outgoing_includes = {},
  };

  for (const auto& symbol : loaded.graph.symbols) {
    if (symbol.file_id == file->id) {
      result.definitions.push_back(symbol.symbol);
    }
  }

  for (const auto& include : loaded.graph.includes) {
    if (include.source_file_id == file->id) {
      result.outgoing_includes.push_back(to_include_record(include, file_paths));
      continue;
    }

    if (include.resolution == ResolutionStatus::kResolved && include.target_file_id &&
        *include.target_file_id == file->id) {
      result.incoming_includes.push_back(to_include_record(include, file_paths));
      continue;
    }

    if (include.resolution == ResolutionStatus::kAmbiguous) {
      for (const auto& candidate : include.candidates) {
        if (candidate.node_id == file->id) {
          result.incoming_includes.push_back(to_include_record(include, file_paths));
          break;
        }
      }
    }
  }

  std::ranges::sort(result.definitions, {}, [](const QuerySymbol& symbol) {
    return std::tuple(symbol.qualified_name, symbol.file_path, symbol.range.start_byte,
                      symbol.signature);
  });
  std::ranges::sort(result.incoming_includes, include_less);
  std::ranges::sort(result.outgoing_includes, include_less);
  return result;
}

}  // namespace sherpa
