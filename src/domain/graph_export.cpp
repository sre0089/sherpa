#include "sherpa/domain/graph_export.hpp"

namespace sherpa {

const char* to_string(ExportNodeKind kind) {
  switch (kind) {
    case ExportNodeKind::kFile:
      return "file";
    case ExportNodeKind::kSymbol:
      return "symbol";
  }
  return "unknown";
}

}  // namespace sherpa
