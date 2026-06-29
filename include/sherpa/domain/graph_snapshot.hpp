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

struct GraphCandidate {
  GraphNodeId node_id{};
  std::string reason;
  std::uint32_t rank{};
};

struct GraphCallEdge {
  GraphNodeId source_symbol_id{};
  std::optional<GraphNodeId> target_symbol_id;
  std::string target_text;
  ResolutionStatus resolution{};
  Confidence confidence{};
  std::string provenance;
  SourceRange evidence;
  std::vector<GraphCandidate> candidates;
};

struct GraphIncludeEdge {
  GraphNodeId source_file_id{};
  std::optional<GraphNodeId> target_file_id;
  std::string target_text;
  ResolutionStatus resolution{};
  Confidence confidence{};
  std::string provenance;
  SourceRange evidence;
  std::vector<GraphCandidate> candidates;
};

struct GraphSnapshot {
  std::vector<GraphFileNode> files;
  std::vector<GraphSymbolNode> symbols;
  std::vector<GraphCallEdge> calls;
  std::vector<GraphIncludeEdge> includes;
};

}  // namespace sherpa
