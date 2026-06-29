#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "sherpa/domain/graph_snapshot.hpp"

namespace sherpa {

struct LoadedQueryGraph {
  std::filesystem::path repository_path;
  std::filesystem::path database_path;
  GraphSnapshot graph;
};

[[nodiscard]] LoadedQueryGraph load_query_graph(const std::filesystem::path& repository_path,
                                                const std::filesystem::path& database_path);

struct SymbolSelectionCriteria {
  std::string name;
  std::string signature;
  std::string file_path;
};

[[nodiscard]] const GraphSymbolNode& select_query_symbol(
    const GraphSnapshot& graph, const SymbolSelectionCriteria& criteria,
    const std::filesystem::path& repository_path, std::string_view not_found_message,
    std::string_view ambiguous_message);

[[nodiscard]] std::string normalize_repository_relative_path(
    const std::string& target, const std::filesystem::path& repository_path);

}  // namespace sherpa
