#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sherpa {

struct SourceRange {
  std::uint32_t start_line{};
  std::uint32_t start_column{};
  std::uint32_t end_line{};
  std::uint32_t end_column{};
  std::uint32_t start_byte{};
  std::uint32_t end_byte{};
};

enum class SymbolKind {
  kFunction,
  kMethod,
  kClass,
  kStruct,
};

enum class SymbolRole {
  kDeclaration,
  kDefinition,
};

enum class DiagnosticSeverity {
  kWarning,
  kError,
};

enum class AnalysisStatus {
  kParsed,
  kFailed,
};

struct SymbolRecord {
  SymbolKind kind{};
  SymbolRole role{};
  std::string name;
  std::string qualified_name;
  std::string signature;
  SourceRange range;
};

struct IncludeRecord {
  std::string target;
  bool is_system{};
  SourceRange range;
};

enum class CallForm {
  kUnqualified,
  kQualified,
  kMember,
  kIndirect,
};

struct CallSiteRecord {
  std::string caller_qualified_name;
  std::uint32_t caller_start_byte{};
  std::string callee_text;
  std::string callee_name;
  CallForm form{};
  std::uint32_t argument_count{};
  SourceRange range;
};

struct DiagnosticRecord {
  DiagnosticSeverity severity{};
  std::string code;
  std::string message;
  SourceRange range;
};

struct FileAnalysis {
  AnalysisStatus status{AnalysisStatus::kParsed};
  std::vector<SymbolRecord> symbols;
  std::vector<IncludeRecord> includes;
  std::vector<CallSiteRecord> calls;
  std::vector<DiagnosticRecord> diagnostics;
};

[[nodiscard]] const char* to_string(SymbolKind kind);
[[nodiscard]] const char* to_string(SymbolRole role);
[[nodiscard]] const char* to_string(DiagnosticSeverity severity);
[[nodiscard]] const char* to_string(AnalysisStatus status);
[[nodiscard]] const char* to_string(CallForm form);

}  // namespace sherpa
