#include "sherpa/application/index_service.hpp"

#include <cstdlib>
#include <stdexcept>

#include "sherpa/scanner/repository_scanner.hpp"
#include "sherpa/storage/sqlite_database.hpp"
#include "sherpa/util/fingerprint.hpp"

namespace sherpa {
namespace {

std::filesystem::path cache_root() {
#if defined(_WIN32)
  if (const char* local_app_data = std::getenv("LOCALAPPDATA")) {
    return std::filesystem::path(local_app_data) / "Sherpa" / "Cache";
  }
#elif defined(__APPLE__)
  if (const char* home = std::getenv("HOME")) {
    return std::filesystem::path(home) / "Library" / "Caches" / "Sherpa";
  }
#else
  if (const char* xdg_cache = std::getenv("XDG_CACHE_HOME")) {
    return std::filesystem::path(xdg_cache) / "sherpa";
  }
  if (const char* home = std::getenv("HOME")) {
    return std::filesystem::path(home) / ".cache" / "sherpa";
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
  const auto database_path =
      options.database_path.empty() ? default_database_path(repository_path) : options.database_path;

  RepositoryScanner scanner;
  auto scan_result = scanner.scan(repository_path);

  SqliteDatabase database(database_path);
  database.initialize_schema();
  database.replace_file_index(repository_path, scan_result.files);

  return IndexResult{
      .repository_path = repository_path,
      .database_path = std::filesystem::absolute(database_path),
      .indexed_files = scan_result.files.size(),
      .warnings = std::move(scan_result.warnings),
  };
}

}  // namespace sherpa
