#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "sherpa/domain/graph_snapshot.hpp"

namespace sherpa {

struct LoadedQueryGraph {
  std::filesystem::path repository_path;
  std::filesystem::path database_path;
  GraphSnapshot graph;
};

[[nodiscard]] LoadedQueryGraph load_query_graph(const std::filesystem::path& repository_path,
                                                const std::filesystem::path& database_path);

[[nodiscard]] std::vector<const GraphSymbolNode*> find_query_symbols(const GraphSnapshot& graph,
                                                                     const std::string& query);

}  // namespace sherpa
