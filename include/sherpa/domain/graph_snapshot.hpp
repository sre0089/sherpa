#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sherpa/domain/call_query.hpp"

namespace sherpa {

using GraphNodeId = std::int64_t;

struct GraphFileNode {
  GraphNodeId id{};
  std::string path;
};

struct GraphSymbolNode {
  GraphNodeId id{};
  GraphNodeId file_id{};
  QuerySymbol symbol;
};

struct GraphCallEdge {
  GraphNodeId source_symbol_id{};
  std::optional<GraphNodeId> target_symbol_id;
  std::string target_text;
  ResolutionStatus resolution{};
  Confidence confidence{};
  std::string provenance;
  SourceRange evidence;
  std::vector<GraphNodeId> candidate_symbol_ids;
};

struct GraphIncludeEdge {
  GraphNodeId source_file_id{};
  std::optional<GraphNodeId> target_file_id;
  std::string target_text;
  ResolutionStatus resolution{};
  Confidence confidence{};
  std::string provenance;
  SourceRange evidence;
  std::vector<GraphNodeId> candidate_file_ids;
};

struct GraphSnapshot {
  std::vector<GraphFileNode> files;
  std::vector<GraphSymbolNode> symbols;
  std::vector<GraphCallEdge> calls;
  std::vector<GraphIncludeEdge> includes;
};

}  // namespace sherpa
