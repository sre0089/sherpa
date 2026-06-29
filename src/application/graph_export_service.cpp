#include "sherpa/application/graph_export_service.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>

#include "graph_query_support.hpp"
#include "sherpa/domain/graph_snapshot.hpp"
#include "sherpa/version.hpp"

namespace sherpa {
namespace {

std::string component(const std::string& value) {
  return std::to_string(value.size()) + ':' + value;
}

std::string file_node_id(const std::string& path) { return "file:" + component(path); }

std::string symbol_node_id(const GraphSymbolNode& symbol) {
  return "symbol:" + component(symbol.symbol.file_path) + component(symbol.symbol.kind) +
         std::to_string(symbol.symbol.range.start_byte) + ':' +
         component(symbol.symbol.qualified_name);
}

std::string edge_id(RelationshipKind kind, const std::string& source_id,
                    const std::string& target_text, const SourceRange& evidence) {
  return "edge:" + component(to_string(kind)) + component(source_id) +
         std::to_string(evidence.start_byte) + ':' + component(target_text);
}

std::string require_node_id(const std::map<GraphNodeId, std::string>& ids, GraphNodeId id) {
  const auto found = ids.find(id);
  if (found == ids.end()) {
    throw std::runtime_error("graph relationship references an unknown export node");
  }
  return found->second;
}

std::vector<GraphExportCandidate> export_candidates(
    const std::vector<GraphCandidate>& candidates,
    const std::map<GraphNodeId, std::string>& node_ids) {
  std::vector<GraphExportCandidate> result;
  result.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    result.push_back(GraphExportCandidate{
        .node_id = require_node_id(node_ids, candidate.node_id),
        .reason = candidate.reason,
        .rank = candidate.rank,
    });
  }
  return result;
}

}  // namespace

GraphExport GraphExportService::build(const GraphExportOptions& options) const {
  const auto loaded = load_query_graph(options.repository_path, options.database_path);
  GraphExport result{
      .schema_version = 1,
      .generator_version = SHERPA_VERSION,
      .repository_root = ".",
      .nodes = {},
      .edges = {},
  };

  std::map<GraphNodeId, std::string> file_ids;
  std::map<GraphNodeId, std::string> symbol_ids;
  for (const auto& file : loaded.graph.files) {
    auto id = file_node_id(file.path);
    file_ids.emplace(file.id, id);
    result.nodes.push_back(GraphExportNode{
        .id = std::move(id),
        .kind = ExportNodeKind::kFile,
        .file_path = file.path,
        .symbol = std::nullopt,
    });
  }
  for (const auto& symbol : loaded.graph.symbols) {
    auto id = symbol_node_id(symbol);
    symbol_ids.emplace(symbol.id, id);
    result.nodes.push_back(GraphExportNode{
        .id = std::move(id),
        .kind = ExportNodeKind::kSymbol,
        .file_path = symbol.symbol.file_path,
        .symbol = symbol.symbol,
    });
  }

  for (const auto& symbol : loaded.graph.symbols) {
    const auto source_id = require_node_id(file_ids, symbol.file_id);
    const auto target_id = require_node_id(symbol_ids, symbol.id);
    result.edges.push_back(GraphExportEdge{
        .id = edge_id(RelationshipKind::kDefines, source_id, target_id, symbol.symbol.range),
        .kind = RelationshipKind::kDefines,
        .source_id = source_id,
        .target_id = target_id,
        .target_text = symbol.symbol.qualified_name,
        .resolution = ResolutionStatus::kResolved,
        .confidence = Confidence::kHigh,
        .provenance = "indexed-definition",
        .evidence = symbol.symbol.range,
        .candidates = {},
    });
  }
  for (const auto& call : loaded.graph.calls) {
    const auto source_id = require_node_id(symbol_ids, call.source_symbol_id);
    const auto target_id =
        call.target_symbol_id
            ? std::optional<std::string>{require_node_id(symbol_ids, *call.target_symbol_id)}
            : std::nullopt;
    result.edges.push_back(GraphExportEdge{
        .id = edge_id(RelationshipKind::kCalls, source_id, call.target_text, call.evidence),
        .kind = RelationshipKind::kCalls,
        .source_id = source_id,
        .target_id = target_id,
        .target_text = call.target_text,
        .resolution = call.resolution,
        .confidence = call.confidence,
        .provenance = call.provenance,
        .evidence = call.evidence,
        .candidates = export_candidates(call.candidates, symbol_ids),
    });
  }
  for (const auto& include : loaded.graph.includes) {
    const auto source_id = require_node_id(file_ids, include.source_file_id);
    const auto target_id =
        include.target_file_id
            ? std::optional<std::string>{require_node_id(file_ids, *include.target_file_id)}
            : std::nullopt;
    result.edges.push_back(GraphExportEdge{
        .id =
            edge_id(RelationshipKind::kIncludes, source_id, include.target_text, include.evidence),
        .kind = RelationshipKind::kIncludes,
        .source_id = source_id,
        .target_id = target_id,
        .target_text = include.target_text,
        .resolution = include.resolution,
        .confidence = include.confidence,
        .provenance = include.provenance,
        .evidence = include.evidence,
        .candidates = export_candidates(include.candidates, file_ids),
    });
  }

  std::ranges::sort(result.nodes, {}, [](const GraphExportNode& node) { return node.id; });
  std::ranges::sort(result.edges, {}, [](const GraphExportEdge& edge) { return edge.id; });
  if (std::ranges::adjacent_find(result.nodes, {}, [](const GraphExportNode& node) {
        return node.id;
      }) != result.nodes.end()) {
    throw std::runtime_error("graph export produced duplicate node identifiers");
  }
  if (std::ranges::adjacent_find(result.edges, {}, [](const GraphExportEdge& edge) {
        return edge.id;
      }) != result.edges.end()) {
    throw std::runtime_error("graph export produced duplicate edge identifiers");
  }
  return result;
}

}  // namespace sherpa
