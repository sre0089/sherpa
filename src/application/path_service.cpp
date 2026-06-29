#include "sherpa/application/path_service.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "graph_query_support.hpp"
#include "sherpa/domain/graph_snapshot.hpp"

namespace sherpa {
namespace {

const GraphSymbolNode& select_symbol(const GraphSnapshot& graph, const std::string& query,
                                     const char* endpoint) {
  const auto matches = find_query_symbols(graph, query);
  if (matches.empty()) {
    throw SymbolNotFoundError(std::string(endpoint) + " symbol not found: " + query);
  }
  if (matches.size() > 1) {
    std::vector<QuerySymbol> candidates;
    candidates.reserve(matches.size());
    for (const auto* match : matches) {
      candidates.push_back(match->symbol);
    }
    throw AmbiguousSymbolError(std::string(endpoint) + " symbol is ambiguous: " + query,
                               std::move(candidates));
  }
  return *matches.front();
}

struct TraversableEdge {
  GraphNodeId target_id{};
  const GraphCallEdge* relationship{};
};

struct Predecessor {
  GraphNodeId source_id{};
  const GraphCallEdge* relationship{};
};

std::optional<std::vector<PathStep>> shortest_path(const GraphSnapshot& graph,
                                                   GraphNodeId source_id, GraphNodeId target_id,
                                                   bool include_ambiguous) {
  if (source_id == target_id) {
    return std::vector<PathStep>{};
  }

  std::map<GraphNodeId, const GraphSymbolNode*> symbols;
  for (const auto& symbol : graph.symbols) {
    symbols.emplace(symbol.id, &symbol);
  }

  std::map<GraphNodeId, std::vector<TraversableEdge>> adjacency;
  for (const auto& call : graph.calls) {
    if (call.resolution == ResolutionStatus::kResolved && call.target_symbol_id) {
      adjacency[call.source_symbol_id].push_back(TraversableEdge{
          .target_id = *call.target_symbol_id,
          .relationship = &call,
      });
    } else if (include_ambiguous && call.resolution == ResolutionStatus::kAmbiguous) {
      for (const auto candidate_id : call.candidate_symbol_ids) {
        adjacency[call.source_symbol_id].push_back(TraversableEdge{
            .target_id = candidate_id,
            .relationship = &call,
        });
      }
    }
  }

  for (auto& [unused, edges] : adjacency) {
    static_cast<void>(unused);
    std::ranges::sort(edges, [&](const TraversableEdge& left, const TraversableEdge& right) {
      const auto& left_symbol = symbols.at(left.target_id)->symbol;
      const auto& right_symbol = symbols.at(right.target_id)->symbol;
      return std::tuple(left.relationship->resolution, left_symbol.qualified_name,
                        left_symbol.file_path, left_symbol.range.start_byte,
                        left.relationship->evidence.start_line,
                        left.relationship->evidence.start_column, left.relationship->confidence,
                        left.relationship->provenance) <
             std::tuple(right.relationship->resolution, right_symbol.qualified_name,
                        right_symbol.file_path, right_symbol.range.start_byte,
                        right.relationship->evidence.start_line,
                        right.relationship->evidence.start_column, right.relationship->confidence,
                        right.relationship->provenance);
    });
  }

  std::set<GraphNodeId> visited{source_id};
  std::map<GraphNodeId, Predecessor> predecessors;
  std::deque<GraphNodeId> queue{source_id};
  bool found = false;
  while (!queue.empty() && !found) {
    const auto current = queue.front();
    queue.pop_front();
    const auto outgoing = adjacency.find(current);
    if (outgoing == adjacency.end()) {
      continue;
    }
    for (const auto& edge : outgoing->second) {
      if (!visited.insert(edge.target_id).second) {
        continue;
      }
      predecessors.emplace(edge.target_id, Predecessor{
                                               .source_id = current,
                                               .relationship = edge.relationship,
                                           });
      if (edge.target_id == target_id) {
        found = true;
        break;
      }
      queue.push_back(edge.target_id);
    }
  }
  if (!found) {
    return std::nullopt;
  }

  std::vector<PathStep> reversed;
  auto current = target_id;
  while (current != source_id) {
    const auto& predecessor = predecessors.at(current);
    const auto& relationship = *predecessor.relationship;
    reversed.push_back(PathStep{
        .source = symbols.at(predecessor.source_id)->symbol,
        .target = symbols.at(current)->symbol,
        .resolution = relationship.resolution,
        .confidence = relationship.confidence,
        .provenance = relationship.provenance,
        .evidence = relationship.evidence,
    });
    current = predecessor.source_id;
  }
  std::ranges::reverse(reversed);
  return reversed;
}

}  // namespace

PathResult PathService::find(const PathOptions& options) const {
  if (options.source.empty()) {
    throw std::invalid_argument("source symbol is required");
  }
  if (options.target.empty()) {
    throw std::invalid_argument("target symbol is required");
  }

  const auto loaded = load_query_graph(options.repository_path, options.database_path);
  const auto& source = select_symbol(loaded.graph, options.source, "source");
  const auto& target = select_symbol(loaded.graph, options.target, "target");

  if (auto steps = shortest_path(loaded.graph, source.id, target.id, false)) {
    return PathResult{
        .source = source.symbol,
        .target = target.symbol,
        .status = PathStatus::kConfirmed,
        .steps = std::move(*steps),
    };
  }
  if (auto steps = shortest_path(loaded.graph, source.id, target.id, true)) {
    return PathResult{
        .source = source.symbol,
        .target = target.symbol,
        .status = PathStatus::kPossible,
        .steps = std::move(*steps),
    };
  }
  return PathResult{
      .source = source.symbol,
      .target = target.symbol,
      .status = PathStatus::kNotFound,
      .steps = {},
  };
}

}  // namespace sherpa
