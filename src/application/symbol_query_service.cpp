#include "sherpa/application/symbol_query_service.hpp"

#include <stdexcept>

#include "graph_query_support.hpp"

namespace sherpa {

SymbolQueryResult SymbolQueryService::query(const SymbolQueryOptions& options) const {
  if (options.symbol.empty()) {
    throw std::invalid_argument("symbol is required");
  }

  const auto loaded = load_query_graph(options.repository_path, options.database_path);
  const auto& symbol = select_query_symbol(
      loaded.graph,
      {.name = options.symbol, .signature = options.signature, .file_path = options.file_path},
      loaded.repository_path, "symbol not found: " + options.symbol,
      "symbol is ambiguous: " + options.symbol);

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
