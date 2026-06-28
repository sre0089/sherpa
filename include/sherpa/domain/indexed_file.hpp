#pragma once

#include "sherpa/domain/file_record.hpp"
#include "sherpa/domain/syntax.hpp"

namespace sherpa {

struct IndexedFile {
  FileRecord file;
  FileAnalysis analysis;
};

}  // namespace sherpa
