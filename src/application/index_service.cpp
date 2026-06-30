#include "sherpa/application/index_service.hpp"

#include <chrono>
#include <cstdlib>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "sherpa/analysis/relationship_resolver.hpp"
#include "sherpa/domain/indexed_file.hpp"
#include "sherpa/parsing/tree_sitter_frontend.hpp"
#include "sherpa/scanner/repository_scanner.hpp"
#include "sherpa/storage/sqlite_database.hpp"
#include "sherpa/util/fingerprint.hpp"

namespace sherpa {
namespace {

using Clock = std::chrono::steady_clock;

std::uint64_t elapsed_milliseconds(Clock::time_point start, Clock::time_point end) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
}

std::optional<std::string> environment_variable(const char* name) {
#if defined(_WIN32)
  char* value = nullptr;
  std::size_t value_size = 0;
  if (_dupenv_s(&value, &value_size, name) != 0 || value == nullptr) {
    return std::nullopt;
  }
  std::string result(value);
  std::free(value);
  return result;
#else
  if (const char* value = std::getenv(name)) {
    return std::string(value);
  }
  return std::nullopt;
#endif
}

std::filesystem::path cache_root() {
#if defined(_WIN32)
  if (const auto local_app_data = environment_variable("LOCALAPPDATA")) {
    return std::filesystem::path(*local_app_data) / "Sherpa" / "Cache";
  }
#elif defined(__APPLE__)
  if (const auto home = environment_variable("HOME")) {
    return std::filesystem::path(*home) / "Library" / "Caches" / "Sherpa";
  }
#else
  if (const auto xdg_cache = environment_variable("XDG_CACHE_HOME")) {
    return std::filesystem::path(*xdg_cache) / "sherpa";
  }
  if (const auto home = environment_variable("HOME")) {
    return std::filesystem::path(*home) / ".cache" / "sherpa";
  }
#endif
  return std::filesystem::temp_directory_path() / "sherpa";
}

}  // namespace

std::filesystem::path IndexService::default_database_path(
    const std::filesystem::path& repository_path) {
  const auto canonical_path = std::filesystem::canonical(repository_path);
  return cache_root() / "repos" / fingerprint(canonical_path.generic_string()) / "index.sqlite";
}

IndexResult IndexService::index(const IndexOptions& options) const {
  const auto total_start = Clock::now();
  if (options.repository_path.empty()) {
    throw std::invalid_argument("repository path is required");
  }
  if (!std::filesystem::exists(options.repository_path)) {
    throw std::invalid_argument("repository path does not exist: " +
                                options.repository_path.string());
  }
  if (!std::filesystem::is_directory(options.repository_path)) {
    throw std::invalid_argument("repository path is not a directory: " +
                                options.repository_path.string());
  }

  const auto repository_path = std::filesystem::canonical(options.repository_path);
  const auto database_path = options.database_path.empty() ? default_database_path(repository_path)
                                                           : options.database_path;

  const auto scan_start = Clock::now();
  RepositoryScanner scanner;
  auto scan_result = scanner.scan(repository_path);
  const auto scan_end = Clock::now();

  const auto cache_load_start = Clock::now();
  SqliteDatabase database(database_path);
  database.initialize_schema();
  auto previous_files = database.load_indexed_files(repository_path);
  std::map<std::string, IndexedFile> previous_by_path;
  for (auto& previous_file : previous_files) {
    previous_by_path.emplace(previous_file.file.relative_path, std::move(previous_file));
  }
  const auto cache_load_end = Clock::now();

  const auto parse_start = Clock::now();
  TreeSitterFrontend frontend;
  std::vector<IndexedFile> indexed_files;
  indexed_files.reserve(scan_result.files.size());
  std::size_t added_file_count = 0;
  std::size_t modified_file_count = 0;
  std::size_t unchanged_file_count = 0;
  std::size_t symbol_count = 0;
  std::size_t include_count = 0;
  std::size_t diagnostic_count = 0;
  for (auto& file : scan_result.files) {
    FileAnalysis analysis;
    const auto previous = previous_by_path.find(file.relative_path);
    if (previous != previous_by_path.end() &&
        previous->second.file.content_fingerprint == file.content_fingerprint &&
        previous->second.file.language == file.language) {
      analysis = std::move(previous->second.analysis);
      ++unchanged_file_count;
      previous_by_path.erase(previous);
    } else {
      if (previous == previous_by_path.end()) {
        ++added_file_count;
      } else {
        ++modified_file_count;
        previous_by_path.erase(previous);
      }
      try {
        analysis = frontend.analyze(file);
      } catch (const std::exception& error) {
        analysis.status = AnalysisStatus::kFailed;
        analysis.diagnostics.push_back(DiagnosticRecord{
            .severity = DiagnosticSeverity::kError,
            .code = "analysis-failed",
            .message = error.what(),
            .range = {},
        });
      }
    }
    symbol_count += analysis.symbols.size();
    include_count += analysis.includes.size();
    diagnostic_count += analysis.diagnostics.size();
    indexed_files.push_back(IndexedFile{
        .file = std::move(file),
        .analysis = std::move(analysis),
    });
  }
  const auto deleted_file_count = previous_by_path.size();
  const auto parse_end = Clock::now();

  const auto relationship_start = Clock::now();
  const auto relationships = RelationshipResolver{}.resolve(indexed_files);
  std::size_t resolved_relationship_count = 0;
  std::size_t ambiguous_relationship_count = 0;
  std::size_t unresolved_relationship_count = 0;
  for (const auto& relationship : relationships) {
    switch (relationship.resolution) {
      case ResolutionStatus::kResolved:
        ++resolved_relationship_count;
        break;
      case ResolutionStatus::kAmbiguous:
        ++ambiguous_relationship_count;
        break;
      case ResolutionStatus::kUnresolved:
        ++unresolved_relationship_count;
        break;
    }
  }
  const auto relationship_end = Clock::now();

  const auto persistence_start = Clock::now();
  database.replace_index(repository_path, indexed_files, relationships);
  const auto persistence_end = Clock::now();

  return IndexResult{
      .repository_path = repository_path,
      .database_path = std::filesystem::absolute(database_path),
      .indexed_files = indexed_files.size(),
      .added_files = added_file_count,
      .modified_files = modified_file_count,
      .unchanged_files = unchanged_file_count,
      .deleted_files = deleted_file_count,
      .parsed_files = added_file_count + modified_file_count,
      .extracted_symbols = symbol_count,
      .extracted_includes = include_count,
      .diagnostics = diagnostic_count,
      .relationships = relationships.size(),
      .resolved_relationships = resolved_relationship_count,
      .ambiguous_relationships = ambiguous_relationship_count,
      .unresolved_relationships = unresolved_relationship_count,
      .scan_milliseconds = elapsed_milliseconds(scan_start, scan_end),
      .cache_load_milliseconds = elapsed_milliseconds(cache_load_start, cache_load_end),
      .parse_milliseconds = elapsed_milliseconds(parse_start, parse_end),
      .relationship_milliseconds =
          elapsed_milliseconds(relationship_start, relationship_end),
      .persistence_milliseconds =
          elapsed_milliseconds(persistence_start, persistence_end),
      .total_milliseconds = elapsed_milliseconds(total_start, Clock::now()),
      .warnings = std::move(scan_result.warnings),
  };
}

}  // namespace sherpa
