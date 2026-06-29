#include "graph_query_support.hpp"

#include <filesystem>
#include <stdexcept>

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

std::vector<const GraphSymbolNode*> find_query_symbols(const GraphSnapshot& graph,
                                                       const std::string& query) {
  std::vector<const GraphSymbolNode*> matches;
  for (const auto& symbol : graph.symbols) {
    if (symbol.symbol.qualified_name == query) {
      matches.push_back(&symbol);
    }
  }
  if (matches.empty()) {
    for (const auto& symbol : graph.symbols) {
      if (symbol.symbol.name == query) {
        matches.push_back(&symbol);
      }
    }
  }
  return matches;
}

}  // namespace sherpa
