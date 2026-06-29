#pragma once

#include <string>
#include <vector>

#include "sherpa/domain/call_query.hpp"

namespace sherpa {

enum class PathStatus {
  kConfirmed,
  kPossible,
  kNotFound,
};

struct PathStep {
  QuerySymbol source;
  QuerySymbol target;
  ResolutionStatus resolution{};
  Confidence confidence{};
  std::string provenance;
  SourceRange evidence;
};

struct PathResult {
  QuerySymbol source;
  QuerySymbol target;
  PathStatus status{};
  std::vector<PathStep> steps;
};

[[nodiscard]] const char* to_string(PathStatus status);

}  // namespace sherpa
