#include "sherpa/parsing/tree_sitter_frontend.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>

#include "sherpa/util/fingerprint.hpp"

#ifndef SHERPA_SOURCE_DIR
#define SHERPA_SOURCE_DIR "."
#endif

namespace {

sherpa::FileRecord fixture_file(const std::string& relative_path, const std::string& language) {
  const auto root = std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "syntax_cpp";
  const auto absolute_path = root / relative_path;
  return sherpa::FileRecord{
      .absolute_path = absolute_path,
      .relative_path = relative_path,
      .language = language,
      .content_fingerprint = sherpa::fingerprint_file(absolute_path),
      .size_bytes = std::filesystem::file_size(absolute_path),
  };
}

const sherpa::SymbolRecord* find_symbol(const sherpa::FileAnalysis& analysis,
                                        const std::string& qualified_name,
                                        sherpa::SymbolRole role) {
  for (const auto& symbol : analysis.symbols) {
    if (symbol.qualified_name == qualified_name && symbol.role == role) {
      return &symbol;
    }
  }
  return nullptr;
}

}  // namespace

TEST_CASE("C++ frontend extracts classes methods functions and includes") {
  const auto analysis =
      sherpa::TreeSitterFrontend{}.analyze(fixture_file("include/widget.hpp", "cpp"));

  REQUIRE(analysis.diagnostics.empty());
  REQUIRE(analysis.includes.size() == 1);
  CHECK(analysis.includes[0].target == "string");
  CHECK(analysis.includes[0].is_system);

  const auto* widget = find_symbol(analysis, "demo::Widget", sherpa::SymbolRole::kDefinition);
  REQUIRE(widget != nullptr);
  CHECK(widget->kind == sherpa::SymbolKind::kClass);
  CHECK(widget->range.start_line == 9);

  const auto* forward = find_symbol(analysis, "demo::Forward", sherpa::SymbolRole::kDeclaration);
  REQUIRE(forward != nullptr);
  CHECK(forward->kind == sherpa::SymbolKind::kClass);

  const auto* compute =
      find_symbol(analysis, "demo::Widget::compute", sherpa::SymbolRole::kDeclaration);
  REQUIRE(compute != nullptr);
  CHECK(compute->kind == sherpa::SymbolKind::kMethod);
  CHECK(compute->signature == "compute(int value) const");

  const auto* helper = find_symbol(analysis, "demo::helper", sherpa::SymbolRole::kDeclaration);
  REQUIRE(helper != nullptr);
  CHECK(helper->kind == sherpa::SymbolKind::kFunction);
  REQUIRE(find_symbol(analysis, "demo::secondary", sherpa::SymbolRole::kDeclaration) != nullptr);
}

TEST_CASE("C++ frontend extracts out-of-class method definitions") {
  const auto analysis = sherpa::TreeSitterFrontend{}.analyze(fixture_file("src/widget.cpp", "cpp"));

  REQUIRE(analysis.diagnostics.empty());
  REQUIRE(find_symbol(analysis, "demo::Widget::Widget", sherpa::SymbolRole::kDefinition) !=
          nullptr);
  REQUIRE(find_symbol(analysis, "demo::Widget::compute", sherpa::SymbolRole::kDefinition) !=
          nullptr);
  REQUIRE(find_symbol(analysis, "demo::helper", sherpa::SymbolRole::kDefinition) != nullptr);
  REQUIRE(find_symbol(analysis, "demo::(anonymous)::internal_scale",
                      sherpa::SymbolRole::kDefinition) != nullptr);
  REQUIRE(analysis.calls.size() == 1);
  CHECK(analysis.calls[0].caller_qualified_name == "demo::Widget::compute");
  CHECK(analysis.calls[0].callee_text == "internal_scale");
  CHECK(analysis.calls[0].callee_name == "internal_scale");
  CHECK(analysis.calls[0].form == sherpa::CallForm::kUnqualified);
  CHECK(analysis.calls[0].argument_count == 1);
}

TEST_CASE("C frontend extracts declarations definitions and system includes") {
  const auto analysis = sherpa::TreeSitterFrontend{}.analyze(fixture_file("src/arithmetic.c", "c"));

  REQUIRE(analysis.diagnostics.empty());
  REQUIRE(analysis.includes.size() == 1);
  CHECK(analysis.includes[0].target == "stddef.h");
  CHECK(analysis.includes[0].is_system);
  REQUIRE(find_symbol(analysis, "add", sherpa::SymbolRole::kDeclaration) != nullptr);
  REQUIRE(find_symbol(analysis, "add", sherpa::SymbolRole::kDefinition) != nullptr);
}

TEST_CASE("frontend preserves useful results for malformed source") {
  const auto analysis =
      sherpa::TreeSitterFrontend{}.analyze(fixture_file("src/malformed.cpp", "cpp"));

  REQUIRE_FALSE(analysis.diagnostics.empty());
}
