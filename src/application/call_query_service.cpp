#include "sherpa/application/call_query_service.hpp"

#include "graph_query_support.hpp"
#include "sherpa/storage/sqlite_database.hpp"

namespace sherpa {

CallQueryResult CallQueryService::query(const CallQueryOptions& options) const {
  if (options.symbol.empty()) {
    throw std::invalid_argument("symbol is required");
  }
  const auto loaded = load_query_graph(options.repository_path, options.database_path);
  const auto& symbol = select_query_symbol(
      loaded.graph,
      {.name = options.symbol, .signature = options.signature, .file_path = options.file_path},
      loaded.repository_path, "symbol not found: " + options.symbol,
      "symbol is ambiguous: " + options.symbol);
  SqliteDatabase database(loaded.database_path, DatabaseOpenMode::kReadOnly);

  return CallQueryResult{
      .direction = options.direction,
      .symbol = symbol.symbol,
      .calls = database.query_calls(symbol.id, options.direction),
  };
}

}  // namespace sherpa
