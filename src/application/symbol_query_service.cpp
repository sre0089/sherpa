#include "sherpa/application/symbol_query_service.hpp"

#include <stdexcept>

#include "graph_query_support.hpp"

namespace sherpa {
namespace {

const GraphSymbolNode& select_symbol(const GraphSnapshot& graph, const std::string& query) {
  const auto matches = find_query_symbols(graph, query);
  if (matches.empty()) {
    throw SymbolNotFoundError("symbol not found: " + query);
  }
  if (matches.size() > 1) {
    std::vector<QuerySymbol> candidates;
    candidates.reserve(matches.size());
    for (const auto* match : matches) {
      candidates.push_back(match->symbol);
    }
    throw AmbiguousSymbolError("symbol is ambiguous: " + query, std::move(candidates));
  }
  return *matches.front();
}

}  // namespace

SymbolQueryResult SymbolQueryService::query(const SymbolQueryOptions& options) const {
  if (options.symbol.empty()) {
    throw std::invalid_argument("symbol is required");
  }

  const auto loaded = load_query_graph(options.repository_path, options.database_path);
  const auto& symbol = select_symbol(loaded.graph, options.symbol);

  SymbolQueryResult result{
      .symbol = symbol.symbol,
      .callers = {},
      .callees = {},
  };

  for (const auto& call : loaded.graph.calls) {
    if (call.source_symbol_id == symbol.id) {
      switch (call.resolution) {
        case ResolutionStatus::kResolved:
          ++result.callees.resolved;
          break;
        case ResolutionStatus::kAmbiguous:
          ++result.callees.ambiguous;
          break;
        case ResolutionStatus::kUnresolved:
          ++result.callees.unresolved;
          break;
      }
    }

    if (call.resolution == ResolutionStatus::kResolved && call.target_symbol_id &&
        *call.target_symbol_id == symbol.id) {
      ++result.callers.resolved;
      continue;
    }
    if (call.resolution == ResolutionStatus::kAmbiguous) {
      for (const auto& candidate : call.candidates) {
        if (candidate.node_id == symbol.id) {
          ++result.callers.ambiguous;
          break;
        }
      }
    }
  }

  return result;
}

}  // namespace sherpa
