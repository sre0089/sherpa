#include "sherpa/application/query_errors.hpp"

#include <utility>

namespace sherpa {

AmbiguousSymbolError::AmbiguousSymbolError(std::string message, std::vector<QuerySymbol> candidates)
    : std::runtime_error(std::move(message)), candidates_(std::move(candidates)) {}

const std::vector<QuerySymbol>& AmbiguousSymbolError::candidates() const noexcept {
  return candidates_;
}

}  // namespace sherpa
