#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sherpa/domain/call_query.hpp"

namespace sherpa {

struct FileQueryCandidate {
  std::string path;
  std::string reason;
  std::uint32_t rank{};
};

struct FileIncludeRecord {
  std::string source_path;
  std::optional<std::string> target_path;
  std::string target_text;
  ResolutionStatus resolution{};
  Confidence confidence{};
  std::string provenance;
  SourceRange evidence;
  std::vector<FileQueryCandidate> candidates;
};

struct FileQueryResult {
  std::string path;
  std::vector<QuerySymbol> definitions;
  std::vector<FileIncludeRecord> incoming_includes;
  std::vector<FileIncludeRecord> outgoing_includes;
};

}  // namespace sherpa
