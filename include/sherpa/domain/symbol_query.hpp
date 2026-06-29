#pragma once

#include <cstddef>

#include "sherpa/domain/call_query.hpp"

namespace sherpa {

struct SymbolCallCount {
  std::size_t resolved{};
  std::size_t ambiguous{};
  std::size_t unresolved{};
};

struct SymbolQueryResult {
  QuerySymbol symbol;
  SymbolCallCount callers;
  SymbolCallCount callees;
};

}  // namespace sherpa
