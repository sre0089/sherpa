#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sherpa/domain/call_query.hpp"

namespace sherpa {

enum class ExportNodeKind {
  kFile,
  kSymbol,
};

struct GraphExportNode {
  std::string id;
  ExportNodeKind kind{};
  std::string file_path;
  std::optional<QuerySymbol> symbol;
};

struct GraphExportCandidate {
  std::string node_id;
  std::string reason;
  std::uint32_t rank{};
};

struct GraphExportEdge {
  std::string id;
  RelationshipKind kind{};
  std::string source_id;
  std::optional<std::string> target_id;
  std::string target_text;
  ResolutionStatus resolution{};
  Confidence confidence{};
  std::string provenance;
  SourceRange evidence;
  std::vector<GraphExportCandidate> candidates;
};

struct GraphExport {
  std::uint32_t schema_version{1};
  std::string generator_version;
  std::string repository_root{"."};
  std::vector<GraphExportNode> nodes;
  std::vector<GraphExportEdge> edges;
};

[[nodiscard]] const char* to_string(ExportNodeKind kind);

}  // namespace sherpa
