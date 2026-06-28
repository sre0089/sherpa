#include "sherpa/domain/syntax.hpp"

#include <stdexcept>

namespace sherpa {

const char* to_string(SymbolKind kind) {
  switch (kind) {
    case SymbolKind::kFunction:
      return "function";
    case SymbolKind::kMethod:
      return "method";
    case SymbolKind::kClass:
      return "class";
    case SymbolKind::kStruct:
      return "struct";
  }
  throw std::logic_error("unknown symbol kind");
}

const char* to_string(SymbolRole role) {
  switch (role) {
    case SymbolRole::kDeclaration:
      return "declaration";
    case SymbolRole::kDefinition:
      return "definition";
  }
  throw std::logic_error("unknown symbol role");
}

const char* to_string(DiagnosticSeverity severity) {
  switch (severity) {
    case DiagnosticSeverity::kWarning:
      return "warning";
    case DiagnosticSeverity::kError:
      return "error";
  }
  throw std::logic_error("unknown diagnostic severity");
}

const char* to_string(AnalysisStatus status) {
  switch (status) {
    case AnalysisStatus::kParsed:
      return "parsed";
    case AnalysisStatus::kFailed:
      return "failed";
  }
  throw std::logic_error("unknown analysis status");
}

}  // namespace sherpa
