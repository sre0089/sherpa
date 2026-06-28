#include "sherpa/application/call_query_service.hpp"

#include <filesystem>
#include <utility>

#include "sherpa/application/index_service.hpp"
#include "sherpa/storage/sqlite_database.hpp"

namespace sherpa {

AmbiguousSymbolError::AmbiguousSymbolError(std::string message, std::vector<QuerySymbol> candidates)
    : std::runtime_error(std::move(message)), candidates_(std::move(candidates)) {}

const std::vector<QuerySymbol>& AmbiguousSymbolError::candidates() const noexcept {
  return candidates_;
}

CallQueryResult CallQueryService::query(const CallQueryOptions& options) const {
  if (options.symbol.empty()) {
    throw std::invalid_argument("symbol is required");
  }
  if (options.repository_path.empty()) {
    throw std::invalid_argument("repository path is required");
  }
  if (!std::filesystem::exists(options.repository_path)) {
    throw std::invalid_argument("repository path does not exist: " +
                                options.repository_path.string());
  }
  if (!std::filesystem::is_directory(options.repository_path)) {
    throw std::invalid_argument("repository path is not a directory: " +
                                options.repository_path.string());
  }

  const auto repository_path = std::filesystem::canonical(options.repository_path);
  const auto database_path = options.database_path.empty()
                                 ? IndexService::default_database_path(repository_path)
                                 : std::filesystem::absolute(options.database_path);
  if (!std::filesystem::is_regular_file(database_path)) {
    throw IndexUnavailableError("index not found for repository; run `sherpa index " +
                                repository_path.generic_string() + "` first");
  }

  SqliteDatabase database(database_path, DatabaseOpenMode::kReadOnly);
  database.validate_schema();
  const auto canonical_repository_path = repository_path.generic_string();
  if (!database.has_completed_index(canonical_repository_path)) {
    throw IndexUnavailableError("database has no completed index for repository: " +
                                canonical_repository_path);
  }

  const auto matches = database.find_definition_symbols(canonical_repository_path, options.symbol);
  if (matches.empty()) {
    throw SymbolNotFoundError("symbol not found: " + options.symbol);
  }
  if (matches.size() > 1) {
    std::vector<QuerySymbol> candidates;
    candidates.reserve(matches.size());
    for (const auto& match : matches) {
      candidates.push_back(match.symbol);
    }
    throw AmbiguousSymbolError("symbol is ambiguous: " + options.symbol, std::move(candidates));
  }

  return CallQueryResult{
      .direction = options.direction,
      .symbol = matches.front().symbol,
      .calls = database.query_calls(matches.front().id, options.direction),
  };
}

}  // namespace sherpa
