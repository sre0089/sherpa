#include "sherpa/parsing/tree_sitter_frontend.hpp"

#include <tree_sitter/api.h>
#include <tree_sitter/tree-sitter-c.h>
#include <tree_sitter/tree-sitter-cpp.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace sherpa {
namespace {

struct ParserDeleter {
  void operator()(TSParser* parser) const { ts_parser_delete(parser); }
};

struct TreeDeleter {
  void operator()(TSTree* tree) const { ts_tree_delete(tree); }
};

using ParserPtr = std::unique_ptr<TSParser, ParserDeleter>;
using TreePtr = std::unique_ptr<TSTree, TreeDeleter>;

std::string read_source(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("could not read source file: " + path.string());
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  if (!input.eof() && input.fail()) {
    throw std::runtime_error("failed while reading source file: " + path.string());
  }
  return contents.str();
}

std::string_view node_type(TSNode node) { return ts_node_type(node); }

bool is_null(TSNode node) { return ts_node_is_null(node); }

std::string node_text(TSNode node, std::string_view source) {
  if (is_null(node)) {
    return {};
  }
  const auto start = static_cast<std::size_t>(ts_node_start_byte(node));
  const auto end = static_cast<std::size_t>(ts_node_end_byte(node));
  if (start > end || end > source.size()) {
    return {};
  }
  return std::string(source.substr(start, end - start));
}

SourceRange source_range(TSNode node) {
  const TSPoint start = ts_node_start_point(node);
  const TSPoint end = ts_node_end_point(node);
  return SourceRange{
      .start_line = start.row + 1U,
      .start_column = start.column + 1U,
      .end_line = end.row + 1U,
      .end_column = end.column + 1U,
      .start_byte = ts_node_start_byte(node),
      .end_byte = ts_node_end_byte(node),
  };
}

TSNode child_by_field(TSNode node, const char* field) {
  return ts_node_child_by_field_name(node, field, static_cast<std::uint32_t>(std::strlen(field)));
}

bool is_name_node(TSNode node) {
  const auto type = node_type(node);
  return type == "identifier" || type == "field_identifier" || type == "operator_name" ||
         type == "destructor_name" || type == "qualified_identifier" || type == "template_function";
}

std::optional<TSNode> declarator_name_node(TSNode node) {
  if (is_null(node)) {
    return std::nullopt;
  }
  if (is_name_node(node)) {
    return node;
  }

  for (const char* field : {"declarator", "name"}) {
    const TSNode child = child_by_field(node, field);
    if (!is_null(child)) {
      if (auto result = declarator_name_node(child)) {
        return result;
      }
    }
  }

  const auto type = node_type(node);
  if (type == "parameter_list" || type == "argument_list" || type == "compound_statement") {
    return std::nullopt;
  }
  const std::uint32_t child_count = ts_node_named_child_count(node);
  for (std::uint32_t index = 0; index < child_count; ++index) {
    if (auto result = declarator_name_node(ts_node_named_child(node, index))) {
      return result;
    }
  }
  return std::nullopt;
}

std::optional<TSNode> find_function_declarator(TSNode node) {
  if (is_null(node)) {
    return std::nullopt;
  }
  if (node_type(node) == "function_declarator") {
    return node;
  }
  if (node_type(node) == "parameter_list") {
    return std::nullopt;
  }
  const std::uint32_t child_count = ts_node_named_child_count(node);
  for (std::uint32_t index = 0; index < child_count; ++index) {
    if (auto result = find_function_declarator(ts_node_named_child(node, index))) {
      return result;
    }
  }
  return std::nullopt;
}

std::string normalize_space(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  bool pending_space = false;
  for (const char raw_character : value) {
    const auto character = static_cast<unsigned char>(raw_character);
    if (std::isspace(character) != 0) {
      pending_space = !result.empty();
      continue;
    }
    if (pending_space && character != ',' && character != ')' && character != ']' &&
        result.back() != '(' && result.back() != '[' && result.back() != ':') {
      result.push_back(' ');
    }
    result.push_back(static_cast<char>(character));
    pending_space = false;
  }
  return result;
}

std::string join_scopes(const std::vector<std::string>& scopes, std::string_view name) {
  if (scopes.empty()) {
    return std::string(name);
  }
  std::string result;
  for (const auto& scope : scopes) {
    if (!result.empty()) {
      result += "::";
    }
    result += scope;
  }
  if (!name.empty()) {
    result += "::";
    result += name;
  }
  return result;
}

std::string unqualified_name(std::string_view name) {
  const auto separator = name.rfind("::");
  return std::string(separator == std::string_view::npos ? name : name.substr(separator + 2U));
}

std::string qualify_name(const std::vector<std::string>& scopes, std::string_view raw_name,
                         std::size_t class_scope_count) {
  if (raw_name.find("::") == std::string_view::npos) {
    return join_scopes(scopes, raw_name);
  }

  std::vector<std::string> namespace_scopes = scopes;
  while (class_scope_count > 0U && !namespace_scopes.empty()) {
    namespace_scopes.pop_back();
    --class_scope_count;
  }
  return join_scopes(namespace_scopes, raw_name);
}

bool has_class_scope(const std::vector<std::string>& class_scopes) { return !class_scopes.empty(); }

class Extractor {
 public:
  explicit Extractor(std::string_view source) : source_(source) {}

  FileAnalysis extract(TSNode root) {
    visit(root);
    sort_and_deduplicate();
    return std::move(analysis_);
  }

 private:
  void visit(TSNode node) {
    record_diagnostic(node);
    const auto type = node_type(node);

    if (type == "namespace_definition") {
      visit_namespace(node);
      return;
    }
    if (type == "class_specifier" || type == "struct_specifier") {
      visit_class(node, type == "class_specifier" ? SymbolKind::kClass : SymbolKind::kStruct);
      return;
    }
    if (type == "function_definition") {
      record_functions(node, SymbolRole::kDefinition);
    } else if (type == "declaration" || type == "field_declaration") {
      record_functions(node, SymbolRole::kDeclaration);
    } else if (type == "preproc_include") {
      record_include(node);
    }

    visit_children(node);
  }

  void visit_children(TSNode node) {
    const std::uint32_t child_count = ts_node_named_child_count(node);
    for (std::uint32_t index = 0; index < child_count; ++index) {
      visit(ts_node_named_child(node, index));
    }
  }

  void visit_namespace(TSNode node) {
    const TSNode name_node = child_by_field(node, "name");
    std::string name = normalize_space(node_text(name_node, source_));
    if (name.empty()) {
      name = "(anonymous)";
    }
    scopes_.push_back(name);
    visit_children(node);
    scopes_.pop_back();
  }

  void visit_class(TSNode node, SymbolKind kind) {
    const TSNode name_node = child_by_field(node, "name");
    const std::string name = normalize_space(node_text(name_node, source_));
    const TSNode body = child_by_field(node, "body");
    if (!name.empty()) {
      analysis_.symbols.push_back(SymbolRecord{
          .kind = kind,
          .role = is_null(body) ? SymbolRole::kDeclaration : SymbolRole::kDefinition,
          .name = name,
          .qualified_name = join_scopes(scopes_, name),
          .signature = {},
          .range = source_range(node),
      });
    }

    if (!name.empty() && !is_null(body)) {
      scopes_.push_back(name);
      class_scopes_.push_back(name);
      visit(body);
      class_scopes_.pop_back();
      scopes_.pop_back();
      return;
    }
    visit_children(node);
  }

  void record_functions(TSNode container, SymbolRole role) {
    std::vector<TSNode> functions;
    if (role == SymbolRole::kDefinition) {
      if (const auto function = find_function_declarator(child_by_field(container, "declarator"))) {
        functions.push_back(*function);
      }
    } else {
      const std::uint32_t child_count = ts_node_named_child_count(container);
      for (std::uint32_t index = 0; index < child_count; ++index) {
        if (const auto function = find_function_declarator(ts_node_named_child(container, index))) {
          if (std::ranges::none_of(functions, [&function](TSNode existing) {
                return ts_node_start_byte(existing) == ts_node_start_byte(*function);
              })) {
            functions.push_back(*function);
          }
        }
      }
    }

    for (const TSNode function : functions) {
      record_function(container, function, role);
    }
  }

  void record_function(TSNode container, TSNode function, SymbolRole role) {
    const auto name_node = declarator_name_node(function);
    if (!name_node) {
      return;
    }

    const std::string raw_name = normalize_space(node_text(*name_node, source_));
    if (raw_name.empty()) {
      return;
    }
    const bool method = has_class_scope(class_scopes_) || raw_name.find("::") != std::string::npos;
    analysis_.symbols.push_back(SymbolRecord{
        .kind = method ? SymbolKind::kMethod : SymbolKind::kFunction,
        .role = role,
        .name = unqualified_name(raw_name),
        .qualified_name = qualify_name(scopes_, raw_name, class_scopes_.size()),
        .signature = normalize_space(node_text(function, source_)),
        .range = source_range(container),
    });
  }

  void record_include(TSNode node) {
    TSNode path = child_by_field(node, "path");
    if (is_null(path) && ts_node_named_child_count(node) > 0U) {
      path = ts_node_named_child(node, ts_node_named_child_count(node) - 1U);
    }
    std::string target = node_text(path, source_);
    const bool system = target.size() >= 2U && target.front() == '<' && target.back() == '>';
    if (target.size() >= 2U && ((target.front() == '<' && target.back() == '>') ||
                                (target.front() == '"' && target.back() == '"'))) {
      target = target.substr(1U, target.size() - 2U);
    }
    if (!target.empty()) {
      analysis_.includes.push_back(IncludeRecord{
          .target = target,
          .is_system = system,
          .range = source_range(node),
      });
    }
  }

  void record_diagnostic(TSNode node) {
    if (!ts_node_is_error(node) && !ts_node_is_missing(node)) {
      return;
    }
    const bool missing = ts_node_is_missing(node);
    analysis_.diagnostics.push_back(DiagnosticRecord{
        .severity = DiagnosticSeverity::kError,
        .code = missing ? "missing-syntax" : "syntax-error",
        .message =
            missing ? "parser expected " + std::string(node_type(node)) : "unrecognized syntax",
        .range = source_range(node),
    });
  }

  void sort_and_deduplicate() {
    auto symbol_key = [](const SymbolRecord& symbol) {
      return std::tuple(symbol.range.start_byte, symbol.kind, symbol.role, symbol.qualified_name);
    };
    std::ranges::sort(analysis_.symbols,
                      [&symbol_key](const SymbolRecord& left, const SymbolRecord& right) {
                        return symbol_key(left) < symbol_key(right);
                      });
    analysis_.symbols.erase(
        std::unique(analysis_.symbols.begin(), analysis_.symbols.end(),
                    [&symbol_key](const SymbolRecord& left, const SymbolRecord& right) {
                      return symbol_key(left) == symbol_key(right);
                    }),
        analysis_.symbols.end());

    std::ranges::sort(analysis_.includes, {}, [](const IncludeRecord& include) {
      return std::tuple(include.range.start_byte, include.target);
    });
    std::ranges::sort(analysis_.diagnostics, {}, [](const DiagnosticRecord& diagnostic) {
      return std::tuple(diagnostic.range.start_byte, diagnostic.code);
    });
  }

  std::string_view source_;
  FileAnalysis analysis_;
  std::vector<std::string> scopes_;
  std::vector<std::string> class_scopes_;
};

const TSLanguage* language_for(std::string_view language) {
  if (language == "c") {
    return tree_sitter_c();
  }
  if (language == "cpp") {
    return tree_sitter_cpp();
  }
  return nullptr;
}

}  // namespace

FileAnalysis TreeSitterFrontend::analyze(const FileRecord& file) const {
  const TSLanguage* language = language_for(file.language);
  if (language == nullptr) {
    throw std::invalid_argument("unsupported language: " + file.language);
  }

  const std::string source = read_source(file.absolute_path);
  ParserPtr parser(ts_parser_new());
  if (!parser) {
    throw std::runtime_error("could not create tree-sitter parser");
  }
  if (!ts_parser_set_language(parser.get(), language)) {
    throw std::runtime_error("tree-sitter grammar is incompatible with the runtime");
  }

  TreePtr tree(ts_parser_parse_string(parser.get(), nullptr, source.data(),
                                      static_cast<std::uint32_t>(source.size())));
  if (!tree) {
    throw std::runtime_error("tree-sitter could not parse: " + file.relative_path);
  }
  return Extractor(source).extract(ts_tree_root_node(tree.get()));
}

}  // namespace sherpa
