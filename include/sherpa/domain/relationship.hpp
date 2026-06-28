#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sherpa/domain/syntax.hpp"

namespace sherpa {

enum class RelationshipKind {
  kDefines,
  kCalls,
  kIncludes,
};

enum class ResolutionStatus {
  kResolved,
  kAmbiguous,
  kUnresolved,
};

enum class Confidence {
  kHigh,
  kMedium,
  kLow,
};

struct SymbolReference {
  std::string file_path;
  std::string qualified_name;
  SymbolRole role{};
  std::uint32_t start_byte{};
};

struct RelationshipCandidate {
  std::optional<std::string> target_file_path;
  std::optional<SymbolReference> target_symbol;
  std::string reason;
  std::uint32_t rank{};
};

struct RelationshipRecord {
  RelationshipKind kind{};
  std::string source_file_path;
  std::optional<SymbolReference> source_symbol;
  std::optional<std::string> target_file_path;
  std::optional<SymbolReference> target_symbol;
  std::string target_text;
  ResolutionStatus resolution{};
  Confidence confidence{};
  std::string provenance;
  SourceRange evidence;
  std::vector<RelationshipCandidate> candidates;
};

[[nodiscard]] const char* to_string(RelationshipKind kind);
[[nodiscard]] const char* to_string(ResolutionStatus status);
[[nodiscard]] const char* to_string(Confidence confidence);

}  // namespace sherpa
