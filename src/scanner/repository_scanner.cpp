#include "sherpa/scanner/repository_scanner.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <system_error>

#include "sherpa/util/fingerprint.hpp"

namespace sherpa {
namespace {

constexpr std::uintmax_t kMaximumFileSize = 16U * 1024U * 1024U;

bool is_ignored_directory(const std::filesystem::path& path) {
  static constexpr std::array ignored_names = {
      ".git", ".hg", ".svn", ".sherpa", "build", "out", "dist", "node_modules",
  };
  const auto name = path.filename().string();
  return std::ranges::find(ignored_names, name) != ignored_names.end();
}

std::string lowercase(std::string value) {
  std::ranges::transform(value, value.begin(),
                         [](unsigned char character) { return std::tolower(character); });
  return value;
}

std::string language_for(const std::filesystem::path& path) {
  const auto extension = lowercase(path.extension().string());
  if (extension == ".c") {
    return "c";
  }
  return "cpp";
}

bool is_within(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  const auto relative = candidate.lexically_relative(root);
  return !relative.empty() && *relative.begin() != "..";
}

}  // namespace

bool RepositoryScanner::is_supported_source(const std::filesystem::path& path) {
  static constexpr std::array extensions = {
      ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inc",
  };
  const auto extension = lowercase(path.extension().string());
  return std::ranges::find(extensions, extension) != extensions.end();
}

ScanResult RepositoryScanner::scan(const std::filesystem::path& repository_path) const {
  ScanResult result;
  const auto root = std::filesystem::canonical(repository_path);
  std::error_code error;

  std::filesystem::recursive_directory_iterator iterator(
      root, std::filesystem::directory_options::skip_permission_denied, error);
  const std::filesystem::recursive_directory_iterator end;
  if (error) {
    throw std::filesystem::filesystem_error("could not scan repository", root, error);
  }

  while (iterator != end) {
    const auto entry = *iterator;
    if (entry.is_directory(error)) {
      if (error) {
        result.warnings.push_back("could not inspect directory: " + entry.path().string());
        error.clear();
      } else if (is_ignored_directory(entry.path())) {
        iterator.disable_recursion_pending();
      }
      iterator.increment(error);
      if (error) {
        result.warnings.push_back("could not continue at: " + entry.path().string());
        error.clear();
      }
      continue;
    }

    if (!entry.is_regular_file(error) || error || !is_supported_source(entry.path())) {
      error.clear();
      iterator.increment(error);
      error.clear();
      continue;
    }

    const auto canonical_path = std::filesystem::canonical(entry.path(), error);
    if (error || !is_within(root, canonical_path)) {
      result.warnings.push_back("skipped source outside repository: " + entry.path().string());
      error.clear();
      iterator.increment(error);
      error.clear();
      continue;
    }

    const auto size = entry.file_size(error);
    if (error) {
      result.warnings.push_back("could not read file size: " + entry.path().string());
      error.clear();
    } else if (size > kMaximumFileSize) {
      result.warnings.push_back("skipped source larger than 16 MiB: " + entry.path().string());
    } else {
      try {
        result.files.push_back(FileRecord{
            .absolute_path = canonical_path,
            .relative_path = canonical_path.lexically_relative(root).generic_string(),
            .language = language_for(canonical_path),
            .content_fingerprint = fingerprint_file(canonical_path),
            .size_bytes = size,
        });
      } catch (const std::exception& exception) {
        result.warnings.push_back(exception.what());
      }
    }

    iterator.increment(error);
    if (error) {
      result.warnings.push_back("could not continue at: " + entry.path().string());
      error.clear();
    }
  }

  std::ranges::sort(result.files, {}, &FileRecord::relative_path);
  return result;
}

}  // namespace sherpa
