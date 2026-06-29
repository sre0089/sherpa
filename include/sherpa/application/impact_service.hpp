#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "sherpa/application/query_errors.hpp"
#include "sherpa/domain/impact.hpp"

namespace sherpa {

struct ImpactOptions {
  std::string target;
  std::string signature{};
  std::string file_path{};
  std::filesystem::path repository_path{"."};
  std::filesystem::path database_path;
};

class ImpactTargetNotFoundError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class ImpactService {
 public:
  [[nodiscard]] ImpactResult analyze(const ImpactOptions& options) const;
};

}  // namespace sherpa
