#pragma once

#include <filesystem>
#include <string>

#include "sherpa/application/query_errors.hpp"
#include "sherpa/domain/symbol_query.hpp"

namespace sherpa {

struct SymbolQueryOptions {
  std::string symbol;
  std::string signature;
  std::string file_path;
  std::filesystem::path repository_path{"."};
  std::filesystem::path database_path;
};

class SymbolQueryService {
 public:
  [[nodiscard]] SymbolQueryResult query(const SymbolQueryOptions& options) const;
};

}  // namespace sherpa
