#include "sherpa/application/impact_service.hpp"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "sherpa/application/index_service.hpp"
#include "sherpa/domain/graph_snapshot.hpp"
#include "sherpa/storage/sqlite_database.hpp"

namespace sherpa {
namespace {

ImpactNode impact_node(const GraphFileNode& file) {
  return ImpactNode{
      .kind = ImpactNodeKind::kFile,
      .file_path = file.path,
      .symbol = std::nullopt,
  };
}

ImpactNode impact_node(const GraphSymbolNode& symbol) {
  return ImpactNode{
      .kind = ImpactNodeKind::kSymbol,
      .file_path = symbol.symbol.file_path,
      .symbol = symbol.symbol,
  };
}

std::string node_name(const ImpactNode& node) {
  return node.symbol ? node.symbol->qualified_name : node.file_path;
}

void sort_records(std::vector<ImpactRecord>& records) {
  std::ranges::sort(records, {}, [](const ImpactRecord& record) {
    return std::tuple(record.depth, record.node.kind, node_name(record.node), record.node.file_path,
                      record.evidence.start_line, record.evidence.start_column);
  });
}

std::string normalized_target_path(const std::string& target,
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

struct TraversalContext {
  const GraphSnapshot& graph;
  std::map<GraphNodeId, const GraphFileNode*> files;
  std::map<GraphNodeId, const GraphSymbolNode*> symbols;
  std::map<GraphNodeId, std::vector<const GraphCallEdge*>> resolved_callers;
  std::map<GraphNodeId, std::vector<const GraphCallEdge*>> possible_callers;
  std::map<GraphNodeId, std::vector<const GraphIncludeEdge*>> resolved_includers;
  std::map<GraphNodeId, std::vector<const GraphIncludeEdge*>> possible_includers;

  explicit TraversalContext(const GraphSnapshot& snapshot) : graph(snapshot) {
    for (const auto& file : graph.files) {
      files.emplace(file.id, &file);
    }
    for (const auto& symbol : graph.symbols) {
      symbols.emplace(symbol.id, &symbol);
    }
    for (const auto& call : graph.calls) {
      if (call.resolution == ResolutionStatus::kResolved && call.target_symbol_id) {
        resolved_callers[*call.target_symbol_id].push_back(&call);
      } else if (call.resolution == ResolutionStatus::kAmbiguous) {
        for (const auto candidate_id : call.candidate_symbol_ids) {
          possible_callers[candidate_id].push_back(&call);
        }
      }
    }
    for (const auto& include : graph.includes) {
      if (include.resolution == ResolutionStatus::kResolved && include.target_file_id) {
        resolved_includers[*include.target_file_id].push_back(&include);
      } else if (include.resolution == ResolutionStatus::kAmbiguous) {
        for (const auto candidate_id : include.candidate_file_ids) {
          possible_includers[candidate_id].push_back(&include);
        }
      }
    }
  }
};

void traverse_callers(const TraversalContext& context,
                      const std::vector<GraphNodeId>& seed_symbol_ids,
                      std::vector<ImpactRecord>& confirmed, std::vector<ImpactRecord>& possible) {
  std::set<GraphNodeId> visited(seed_symbol_ids.begin(), seed_symbol_ids.end());
  std::map<GraphNodeId, ImpactRecord> confirmed_by_id;
  std::map<GraphNodeId, ImpactRecord> possible_by_id;
  std::deque<std::pair<GraphNodeId, std::uint32_t>> queue;
  for (const auto id : seed_symbol_ids) {
    queue.emplace_back(id, 0);
  }

  while (!queue.empty()) {
    const auto [target_id, depth] = queue.front();
    queue.pop_front();
    const auto target = context.symbols.at(target_id);

    if (const auto found = context.resolved_callers.find(target_id);
        found != context.resolved_callers.end()) {
      for (const auto* edge : found->second) {
        const auto source = context.symbols.at(edge->source_symbol_id);
        if (!visited.insert(source->id).second) {
          continue;
        }
        possible_by_id.erase(source->id);
        confirmed_by_id.emplace(source->id, ImpactRecord{
                                                .node = impact_node(*source),
                                                .dependency = impact_node(*target),
                                                .relationship = RelationshipKind::kCalls,
                                                .confidence = edge->confidence,
                                                .provenance = edge->provenance,
                                                .evidence = edge->evidence,
                                                .depth = depth + 1U,
                                            });
        queue.emplace_back(source->id, depth + 1U);
      }
    }

    if (const auto found = context.possible_callers.find(target_id);
        found != context.possible_callers.end()) {
      for (const auto* edge : found->second) {
        const auto source = context.symbols.at(edge->source_symbol_id);
        if (visited.contains(source->id) || possible_by_id.contains(source->id)) {
          continue;
        }
        possible_by_id.emplace(source->id, ImpactRecord{
                                               .node = impact_node(*source),
                                               .dependency = impact_node(*target),
                                               .relationship = RelationshipKind::kCalls,
                                               .confidence = edge->confidence,
                                               .provenance = edge->provenance,
                                               .evidence = edge->evidence,
                                               .depth = depth + 1U,
                                           });
      }
    }
  }

  for (auto& [unused, record] : confirmed_by_id) {
    static_cast<void>(unused);
    confirmed.push_back(std::move(record));
  }
  for (auto& [unused, record] : possible_by_id) {
    static_cast<void>(unused);
    possible.push_back(std::move(record));
  }
}

void traverse_includers(const TraversalContext& context, GraphNodeId seed_file_id,
                        std::vector<ImpactRecord>& confirmed, std::vector<ImpactRecord>& possible) {
  std::set<GraphNodeId> visited{seed_file_id};
  std::map<GraphNodeId, ImpactRecord> confirmed_by_id;
  std::map<GraphNodeId, ImpactRecord> possible_by_id;
  std::deque<std::pair<GraphNodeId, std::uint32_t>> queue{{seed_file_id, 0}};

  while (!queue.empty()) {
    const auto [target_id, depth] = queue.front();
    queue.pop_front();
    const auto target = context.files.at(target_id);

    if (const auto found = context.resolved_includers.find(target_id);
        found != context.resolved_includers.end()) {
      for (const auto* edge : found->second) {
        const auto source = context.files.at(edge->source_file_id);
        if (!visited.insert(source->id).second) {
          continue;
        }
        possible_by_id.erase(source->id);
        confirmed_by_id.emplace(source->id, ImpactRecord{
                                                .node = impact_node(*source),
                                                .dependency = impact_node(*target),
                                                .relationship = RelationshipKind::kIncludes,
                                                .confidence = edge->confidence,
                                                .provenance = edge->provenance,
                                                .evidence = edge->evidence,
                                                .depth = depth + 1U,
                                            });
        queue.emplace_back(source->id, depth + 1U);
      }
    }

    if (const auto found = context.possible_includers.find(target_id);
        found != context.possible_includers.end()) {
      for (const auto* edge : found->second) {
        const auto source = context.files.at(edge->source_file_id);
        if (visited.contains(source->id) || possible_by_id.contains(source->id)) {
          continue;
        }
        possible_by_id.emplace(source->id, ImpactRecord{
                                               .node = impact_node(*source),
                                               .dependency = impact_node(*target),
                                               .relationship = RelationshipKind::kIncludes,
                                               .confidence = edge->confidence,
                                               .provenance = edge->provenance,
                                               .evidence = edge->evidence,
                                               .depth = depth + 1U,
                                           });
      }
    }
  }

  for (auto& [unused, record] : confirmed_by_id) {
    static_cast<void>(unused);
    confirmed.push_back(std::move(record));
  }
  for (auto& [unused, record] : possible_by_id) {
    static_cast<void>(unused);
    possible.push_back(std::move(record));
  }
}

}  // namespace

ImpactResult ImpactService::analyze(const ImpactOptions& options) const {
  if (options.target.empty()) {
    throw std::invalid_argument("impact target is required");
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

  const auto graph = database.load_graph(canonical_repository_path);
  const TraversalContext context(graph);
  const auto file_path = normalized_target_path(options.target, repository_path);
  const auto file = std::ranges::find(graph.files, file_path,
                                      [](const GraphFileNode& node) { return node.path; });

  ImpactResult result;
  if (file != graph.files.end()) {
    result.target = impact_node(*file);
    traverse_includers(context, file->id, result.confirmed, result.possible);

    std::vector<GraphNodeId> definitions;
    for (const auto& symbol : graph.symbols) {
      if (symbol.file_id == file->id) {
        definitions.push_back(symbol.id);
      }
    }
    traverse_callers(context, definitions, result.confirmed, result.possible);
  } else {
    std::vector<const GraphSymbolNode*> matches;
    for (const auto& symbol : graph.symbols) {
      if (symbol.symbol.qualified_name == options.target) {
        matches.push_back(&symbol);
      }
    }
    if (matches.empty()) {
      for (const auto& symbol : graph.symbols) {
        if (symbol.symbol.name == options.target) {
          matches.push_back(&symbol);
        }
      }
    }
    if (matches.empty()) {
      throw ImpactTargetNotFoundError("file or symbol not found: " + options.target);
    }
    if (matches.size() > 1) {
      std::vector<QuerySymbol> candidates;
      candidates.reserve(matches.size());
      for (const auto* match : matches) {
        candidates.push_back(match->symbol);
      }
      throw AmbiguousSymbolError("symbol is ambiguous: " + options.target, std::move(candidates));
    }

    result.target = impact_node(*matches.front());
    traverse_callers(context, {matches.front()->id}, result.confirmed, result.possible);
  }

  sort_records(result.confirmed);
  sort_records(result.possible);
  return result;
}

}  // namespace sherpa
