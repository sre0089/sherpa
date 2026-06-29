#include "graph_query_support.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "sherpa/application/index_service.hpp"
#include "sherpa/application/query_errors.hpp"
#include "sherpa/storage/sqlite_database.hpp"

namespace sherpa {

LoadedQueryGraph load_query_graph(const std::filesystem::path& repository_path,
                                  const std::filesystem::path& database_path) {
  if (repository_path.empty()) {
    throw std::invalid_argument("repository path is required");
  }
  if (!std::filesystem::exists(repository_path)) {
    throw std::invalid_argument("repository path does not exist: " + repository_path.string());
  }
  if (!std::filesystem::is_directory(repository_path)) {
    throw std::invalid_argument("repository path is not a directory: " + repository_path.string());
  }

  const auto canonical_repository_path = std::filesystem::canonical(repository_path);
  const auto selected_database_path =
      database_path.empty() ? IndexService::default_database_path(canonical_repository_path)
                            : std::filesystem::absolute(database_path);
  if (!std::filesystem::is_regular_file(selected_database_path)) {
    throw IndexUnavailableError("index not found for repository; run `sherpa index " +
                                canonical_repository_path.generic_string() + "` first");
  }

  SqliteDatabase database(selected_database_path, DatabaseOpenMode::kReadOnly);
  database.validate_schema();
  const auto repository_key = canonical_repository_path.generic_string();
  if (!database.has_completed_index(repository_key)) {
    throw IndexUnavailableError("database has no completed index for repository: " +
                                repository_key);
  }

  return LoadedQueryGraph{
      .repository_path = canonical_repository_path,
      .database_path = selected_database_path,
      .graph = database.load_graph(repository_key),
  };
}

const GraphSymbolNode& select_query_symbol(const GraphSnapshot& graph,
                                           const SymbolSelectionCriteria& criteria,
                                           const std::filesystem::path& repository_path,
                                           std::string_view not_found_message,
                                           std::string_view ambiguous_message) {
  std::vector<const GraphSymbolNode*> matches;
  for (const auto& symbol : graph.symbols) {
    if (symbol.symbol.qualified_name == criteria.name) {
      matches.push_back(&symbol);
    }
  }
  if (matches.empty()) {
    for (const auto& symbol : graph.symbols) {
      if (symbol.symbol.name == criteria.name) {
        matches.push_back(&symbol);
      }
    }
  }

  if (!criteria.signature.empty()) {
    std::erase_if(matches, [&](const GraphSymbolNode* symbol) {
      return symbol->symbol.signature != criteria.signature;
    });
  }
  if (!criteria.file_path.empty()) {
    const auto normalized_file =
        normalize_repository_relative_path(criteria.file_path, repository_path);
    std::erase_if(matches, [&](const GraphSymbolNode* symbol) {
      return symbol->symbol.file_path != normalized_file;
    });
  }

  std::ranges::sort(matches, {}, [](const GraphSymbolNode* symbol) {
    return std::tuple(symbol->symbol.qualified_name, symbol->symbol.file_path,
                      symbol->symbol.range.start_byte, symbol->symbol.signature);
  });

  if (matches.empty()) {
    throw SymbolNotFoundError(std::string(not_found_message));
  }
  if (matches.size() > 1) {
    std::vector<QuerySymbol> candidates;
    candidates.reserve(matches.size());
    for (const auto* match : matches) {
      candidates.push_back(match->symbol);
    }
    throw AmbiguousSymbolError(std::string(ambiguous_message) +
                                   "; add an exact signature or file filter to select one "
                                   "definition",
                               std::move(candidates));
  }
  return *matches.front();
}

std::string normalize_repository_relative_path(const std::string& target,
                                               const std::filesystem::path& repository_path) {
  std::filesystem::path path(target);
  if (path.is_absolute()) {
    std::error_code error;
    const auto canonical_path = std::filesystem::weakly_canonical(path, error);
    if (!error) {
      path = canonical_path;
    }
    path = path.lexically_normal().lexically_relative(repository_path);
  } else {
    path = path.lexically_normal();
  }
  auto normalized = path.generic_string();
  if (normalized.starts_with("./")) {
    normalized.erase(0, 2);
  }
  return normalized;
}

}  // namespace sherpa
