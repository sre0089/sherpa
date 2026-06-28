#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "sherpa/domain/call_query.hpp"

namespace sherpa {

struct CallQueryOptions {
  CallQueryDirection direction{};
  std::string symbol;
  std::filesystem::path repository_path{"."};
  std::filesystem::path database_path;
};

class IndexUnavailableError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class SymbolNotFoundError : public std::runtime_error {
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

class CallQueryService {
 public:
  [[nodiscard]] CallQueryResult query(const CallQueryOptions& options) const;
};

}  // namespace sherpa
