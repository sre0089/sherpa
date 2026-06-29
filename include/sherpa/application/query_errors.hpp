#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "sherpa/domain/call_query.hpp"

namespace sherpa {

class IndexUnavailableError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class SymbolNotFoundError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class FileNotFoundError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class AmbiguousSymbolError : public std::runtime_error {
 public:
  AmbiguousSymbolError(std::string message, std::vector<QuerySymbol> candidates);

  [[nodiscard]] const std::vector<QuerySymbol>& candidates() const noexcept;

 private:
  std::vector<QuerySymbol> candidates_;
};

}  // namespace sherpa
