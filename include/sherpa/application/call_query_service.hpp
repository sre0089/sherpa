#pragma once

#include <filesystem>
#include <string>

#include "sherpa/application/query_errors.hpp"
#include "sherpa/domain/call_query.hpp"

namespace sherpa {

struct CallQueryOptions {
  CallQueryDirection direction{};
  std::string symbol;
  std::string signature;
  std::string file_path;
  std::filesystem::path repository_path{"."};
  std::filesystem::path database_path;
};

class CallQueryService {
 public:
  [[nodiscard]] CallQueryResult query(const CallQueryOptions& options) const;
};

}  // namespace sherpa
