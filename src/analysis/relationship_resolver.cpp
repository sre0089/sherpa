#include "sherpa/analysis/relationship_resolver.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sherpa {
namespace {

struct SymbolInfo {
  const IndexedFile* file{};
  const SymbolRecord* symbol{};
};

struct CallResolution {
  std::vector<SymbolInfo> matches;
  Confidence confidence{Confidence::kLow};
  std::string method{"no-match"};
};

struct IncludeResolution {
  std::vector<std::string> matches;
  Confidence confidence{Confidence::kLow};
  std::string method{"no-match"};
};

using SymbolKey = std::pair<std::string, std::string>;

SymbolReference reference(const SymbolInfo& info) {
  return SymbolReference{
      .file_path = info.file->file.relative_path,
      .qualified_name = info.symbol->qualified_name,
      .role = info.symbol->role,
      .start_byte = info.symbol->range.start_byte,
  };
}

bool is_callable_definition(const SymbolRecord& symbol) {
  return symbol.role == SymbolRole::kDefinition &&
         (symbol.kind == SymbolKind::kFunction || symbol.kind == SymbolKind::kMethod);
}

std::string strip_template_arguments(std::string value) {
  std::string result;
  result.reserve(value.size());
  std::uint32_t depth = 0;
  for (const char character : value) {
    if (character == '<') {
      ++depth;
    } else if (character == '>' && depth > 0U) {
      --depth;
    } else if (depth == 0U) {
      result.push_back(character);
    }
  }
  if (result.starts_with("::")) {
    result.erase(0, 2);
  }
  return result;
}

std::vector<std::string> scope_candidates(std::string caller, std::string_view callee) {
  const auto caller_separator = caller.rfind("::");
  if (caller_separator == std::string::npos) {
    return {std::string(callee)};
  }
  caller.erase(caller_separator);

  std::vector<std::string> result;
  while (!caller.empty()) {
    result.push_back(caller + "::" + std::string(callee));
    const auto separator = caller.rfind("::");
    if (separator == std::string::npos) {
      break;
    }
    caller.erase(separator);
  }
  result.emplace_back(callee);
  return result;
}

std::vector<SymbolInfo> find_qualified(
    const std::map<SymbolKey, std::vector<SymbolInfo>>& by_qualified,
    const std::string& language_family, const std::string& qualified_name) {
  const auto found = by_qualified.find({language_family, qualified_name});
  return found == by_qualified.end() ? std::vector<SymbolInfo>{} : found->second;
}

std::string language_family(std::string_view language) {
  return language == "python" ? "python" : "c-family";
}

CallResolution resolve_call_candidates(
    const CallSiteRecord& call, const std::string& family,
    const std::map<SymbolKey, std::vector<SymbolInfo>>& by_qualified,
    const std::map<SymbolKey, std::vector<SymbolInfo>>& by_name) {
  if (call.form == CallForm::kIndirect) {
    return {};
  }

  const std::string callee_text = strip_template_arguments(call.callee_text);
  if (call.form == CallForm::kQualified) {
    if (auto matches = find_qualified(by_qualified, family, callee_text); !matches.empty()) {
      return {.matches = std::move(matches),
              .confidence = Confidence::kHigh,
              .method = "qualified-name-match"};
    }
    for (const auto& candidate : scope_candidates(call.caller_qualified_name, callee_text)) {
      if (auto matches = find_qualified(by_qualified, family, candidate); !matches.empty()) {
        return {.matches = std::move(matches),
                .confidence = Confidence::kHigh,
                .method = "scoped-qualified-match"};
      }
    }
  } else if (call.form == CallForm::kUnqualified) {
    for (const auto& candidate : scope_candidates(call.caller_qualified_name, call.callee_name)) {
      if (auto matches = find_qualified(by_qualified, family, candidate); !matches.empty()) {
        return {.matches = std::move(matches),
                .confidence = Confidence::kHigh,
                .method = "lexical-scope-match"};
      }
    }
  }

  const auto found = by_name.find({family, call.callee_name});
  if (found == by_name.end()) {
    return {};
  }
  return {
      .matches = found->second,
      .confidence = call.form == CallForm::kMember ? Confidence::kLow : Confidence::kMedium,
      .method = call.form == CallForm::kMember ? "member-name-match" : "repository-name-match",
  };
}

std::string normalize_path(const std::filesystem::path& path) {
  return path.lexically_normal().generic_string();
}

bool has_path_suffix(std::string_view path, std::string_view suffix) {
  if (!path.ends_with(suffix)) {
    return false;
  }
  return path.size() == suffix.size() || path[path.size() - suffix.size() - 1U] == '/';
}

std::string python_module_for_path(std::string path) {
  const auto extension = path.rfind('.');
  if (extension != std::string::npos) {
    path.erase(extension);
  }
  std::ranges::replace(path, '/', '.');
  if (path == "__init__") {
    return {};
  }
  constexpr std::string_view init_suffix = ".__init__";
  if (path.ends_with(init_suffix)) {
    path.erase(path.size() - init_suffix.size());
  }
  return path;
}

std::string python_import_module(const IndexedFile& source, std::string target) {
  const auto first_name = target.find_first_not_of('.');
  const std::size_t level = first_name == std::string::npos ? target.size() : first_name;
  if (level == 0U) {
    return target;
  }

  std::string package = python_module_for_path(source.file.relative_path);
  const auto filename = std::filesystem::path(source.file.relative_path).filename().string();
  if (filename != "__init__.py" && filename != "__init__.pyi") {
    const auto separator = package.rfind('.');
    package = separator == std::string::npos ? std::string{} : package.substr(0, separator);
  }
  for (std::size_t parent = 1U; parent < level; ++parent) {
    const auto separator = package.rfind('.');
    if (separator == std::string::npos) {
      package.clear();
      break;
    }
    package.erase(separator);
  }
  const std::string remainder =
      first_name == std::string::npos ? std::string{} : target.substr(first_name);
  if (package.empty()) {
    return remainder;
  }
  return remainder.empty() ? package : package + "." + remainder;
}

IncludeResolution resolve_python_import(
    const IndexedFile& source, const IncludeRecord& include,
    const std::map<std::string, std::vector<std::string>>& files_by_module) {
  const std::string module = python_import_module(source, include.target);
  const auto found = files_by_module.find(module);
  if (found == files_by_module.end()) {
    return {.matches = {}, .confidence = Confidence::kLow, .method = "python-import:no-match"};
  }
  return {.matches = found->second,
          .confidence = found->second.size() == 1U ? Confidence::kHigh : Confidence::kLow,
          .method = "python-import:module-match"};
}

IncludeResolution resolve_include(
    const IndexedFile& source, const IncludeRecord& include,
    const std::map<std::string, const IndexedFile*>& files,
    const std::map<std::string, std::vector<std::string>>& python_files_by_module) {
  if (source.file.language == "python") {
    return resolve_python_import(source, include, python_files_by_module);
  }
  if (include.is_system) {
    return {.matches = {}, .confidence = Confidence::kLow, .method = "system-include"};
  }

  const auto source_directory = std::filesystem::path(source.file.relative_path).parent_path();
  const std::string relative_candidate =
      normalize_path(source_directory / std::filesystem::path(include.target));
  if (files.contains(relative_candidate)) {
    return {.matches = {relative_candidate},
            .confidence = Confidence::kHigh,
            .method = "relative-path-match"};
  }

  const std::string root_candidate = normalize_path(include.target);
  if (files.contains(root_candidate)) {
    return {.matches = {root_candidate},
            .confidence = Confidence::kHigh,
            .method = "repository-root-match"};
  }

  std::vector<std::string> matches;
  for (const auto& [path, unused] : files) {
    static_cast<void>(unused);
    if (has_path_suffix(path, root_candidate)) {
      matches.push_back(path);
    }
  }
  return {.matches = std::move(matches),
          .confidence = Confidence::kMedium,
          .method = "path-suffix-match"};
}

RelationshipCandidate symbol_candidate(const SymbolInfo& info, std::uint32_t rank,
                                       std::string reason) {
  return RelationshipCandidate{
      .target_file_path = std::nullopt,
      .target_symbol = reference(info),
      .reason = std::move(reason),
      .rank = rank,
  };
}

}  // namespace

std::vector<RelationshipRecord> RelationshipResolver::resolve(
    const std::vector<IndexedFile>& files) const {
  std::map<std::string, const IndexedFile*> files_by_path;
  std::map<std::string, std::vector<std::string>> python_files_by_module;
  std::map<SymbolKey, std::vector<SymbolInfo>> definitions_by_qualified;
  std::map<SymbolKey, std::vector<SymbolInfo>> definitions_by_name;
  std::map<std::tuple<std::string, std::string, std::uint32_t>, SymbolInfo> definitions_by_source;

  for (const auto& file : files) {
    files_by_path.emplace(file.file.relative_path, &file);
    if (file.file.language == "python") {
      python_files_by_module[python_module_for_path(file.file.relative_path)].push_back(
          file.file.relative_path);
    }
    const std::string family = language_family(file.file.language);
    for (const auto& symbol : file.analysis.symbols) {
      if (symbol.role == SymbolRole::kDefinition) {
        const SymbolInfo info{.file = &file, .symbol = &symbol};
        definitions_by_source.emplace(
            std::tuple(file.file.relative_path, symbol.qualified_name, symbol.range.start_byte),
            info);
        if (is_callable_definition(symbol)) {
          definitions_by_qualified[{family, symbol.qualified_name}].push_back(info);
          definitions_by_name[{family, symbol.name}].push_back(info);
        }
      }
    }
  }

  const auto symbol_order = [](const SymbolInfo& left, const SymbolInfo& right) {
    return std::tuple(left.symbol->qualified_name, left.file->file.relative_path,
                      left.symbol->range.start_byte) < std::tuple(right.symbol->qualified_name,
                                                                  right.file->file.relative_path,
                                                                  right.symbol->range.start_byte);
  };
  for (auto& [unused, symbols] : definitions_by_qualified) {
    static_cast<void>(unused);
    std::ranges::sort(symbols, symbol_order);
  }
  for (auto& [unused, symbols] : definitions_by_name) {
    static_cast<void>(unused);
    std::ranges::sort(symbols, symbol_order);
  }

  std::vector<RelationshipRecord> relationships;
  for (const auto& file : files) {
    for (const auto& symbol : file.analysis.symbols) {
      if (symbol.role != SymbolRole::kDefinition) {
        continue;
      }
      const SymbolInfo target{.file = &file, .symbol = &symbol};
      relationships.push_back(RelationshipRecord{
          .kind = RelationshipKind::kDefines,
          .source_file_path = file.file.relative_path,
          .source_symbol = std::nullopt,
          .target_file_path = std::nullopt,
          .target_symbol = reference(target),
          .target_text = symbol.qualified_name,
          .resolution = ResolutionStatus::kResolved,
          .confidence = Confidence::kHigh,
          .provenance = "tree-sitter-definition",
          .evidence = symbol.range,
          .candidates = {},
      });
    }

    for (const auto& include : file.analysis.includes) {
      auto include_resolution =
          resolve_include(file, include, files_by_path, python_files_by_module);
      RelationshipRecord relationship{
          .kind = RelationshipKind::kIncludes,
          .source_file_path = file.file.relative_path,
          .source_symbol = std::nullopt,
          .target_file_path = std::nullopt,
          .target_symbol = std::nullopt,
          .target_text = include.target,
          .resolution = ResolutionStatus::kUnresolved,
          .confidence = Confidence::kLow,
          .provenance = include_resolution.method,
          .evidence = include.range,
          .candidates = {},
      };
      if (include_resolution.matches.size() == 1U) {
        relationship.target_file_path = include_resolution.matches.front();
        relationship.resolution = ResolutionStatus::kResolved;
        relationship.confidence = include_resolution.confidence;
      } else if (include_resolution.matches.size() > 1U) {
        relationship.resolution = ResolutionStatus::kAmbiguous;
        relationship.confidence = Confidence::kLow;
        std::uint32_t rank = 1;
        for (auto& candidate : include_resolution.matches) {
          relationship.candidates.push_back(RelationshipCandidate{
              .target_file_path = std::move(candidate),
              .target_symbol = std::nullopt,
              .reason = file.file.language == "python" ? "matching Python module"
                                                       : "repository path suffix match",
              .rank = rank++,
          });
        }
      }
      relationships.push_back(std::move(relationship));
    }

    for (const auto& call : file.analysis.calls) {
      const auto caller_key =
          std::tuple(file.file.relative_path, call.caller_qualified_name, call.caller_start_byte);
      const auto caller = definitions_by_source.find(caller_key);
      const auto call_resolution = resolve_call_candidates(
          call, language_family(file.file.language), definitions_by_qualified, definitions_by_name);
      RelationshipRecord relationship{
          .kind = RelationshipKind::kCalls,
          .source_file_path = file.file.relative_path,
          .source_symbol = caller == definitions_by_source.end()
                               ? std::optional<SymbolReference>{}
                               : std::optional<SymbolReference>{reference(caller->second)},
          .target_file_path = std::nullopt,
          .target_symbol = std::nullopt,
          .target_text = call.callee_text,
          .resolution = ResolutionStatus::kUnresolved,
          .confidence = Confidence::kLow,
          .provenance = std::string("tree-sitter-call:") + to_string(call.form) + ":" +
                        call_resolution.method,
          .evidence = call.range,
          .candidates = {},
      };
      if (call_resolution.matches.size() == 1U) {
        relationship.target_symbol = reference(call_resolution.matches.front());
        relationship.resolution = ResolutionStatus::kResolved;
        relationship.confidence = call_resolution.confidence;
      } else if (call_resolution.matches.size() > 1U) {
        relationship.resolution = ResolutionStatus::kAmbiguous;
        relationship.confidence = call_resolution.confidence;
        std::uint32_t rank = 1;
        for (const auto& match : call_resolution.matches) {
          relationship.candidates.push_back(
              symbol_candidate(match, rank++, "matching callable definition"));
        }
      }
      relationships.push_back(std::move(relationship));
    }
  }

  std::ranges::sort(relationships, {}, [](const RelationshipRecord& relationship) {
    return std::tuple(relationship.source_file_path, relationship.evidence.start_byte,
                      relationship.kind, relationship.target_text);
  });
  return relationships;
}

}  // namespace sherpa
