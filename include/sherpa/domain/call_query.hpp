#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sherpa/domain/relationship.hpp"

namespace sherpa {

enum class CallQueryDirection {
  kCallers,
  kCallees,
};

struct QuerySymbol {
  std::string kind;
  std::string name;
  std::string qualified_name;
  std::string signature;
  std::string file_path;
  SourceRange range;
};

struct CallQueryCandidate {
  QuerySymbol symbol;
  std::string reason;
  std::uint32_t rank{};
};

struct CallQueryEntry {
  QuerySymbol caller;
  std::optional<QuerySymbol> callee;
  std::string target_text;
  ResolutionStatus resolution{};
  Confidence confidence{};
  std::string provenance;
  SourceRange evidence;
  std::vector<CallQueryCandidate> candidates;
};

struct CallQueryResult {
  CallQueryDirection direction{};
  QuerySymbol symbol;
  std::vector<CallQueryEntry> calls;
};

[[nodiscard]] const char* to_string(CallQueryDirection direction);

}  // namespace sherpa
