#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sherpa/domain/call_query.hpp"

namespace sherpa {

enum class ImpactNodeKind {
  kFile,
  kSymbol,
};

struct ImpactNode {
  ImpactNodeKind kind{};
  std::string file_path;
  std::optional<QuerySymbol> symbol;
};

struct ImpactRecord {
  ImpactNode node;
  ImpactNode dependency;
  RelationshipKind relationship{};
  Confidence confidence{};
  std::string provenance;
  SourceRange evidence;
  std::uint32_t depth{};
};

struct ImpactResult {
  ImpactNode target;
  std::vector<ImpactRecord> confirmed;
  std::vector<ImpactRecord> possible;
};

[[nodiscard]] const char* to_string(ImpactNodeKind kind);

}  // namespace sherpa
