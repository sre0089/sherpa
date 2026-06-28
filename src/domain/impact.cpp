#include "sherpa/domain/impact.hpp"

namespace sherpa {

const char* to_string(ImpactNodeKind kind) {
  switch (kind) {
    case ImpactNodeKind::kFile:
      return "file";
    case ImpactNodeKind::kSymbol:
      return "symbol";
  }
  return "unknown";
}

}  // namespace sherpa
