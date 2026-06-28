#pragma once

#include "sherpa/domain/file_record.hpp"
#include "sherpa/domain/syntax.hpp"

namespace sherpa {

class TreeSitterFrontend {
 public:
  [[nodiscard]] FileAnalysis analyze(const FileRecord& file) const;
};

}  // namespace sherpa
