#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "sherpa/application/index_service.hpp"
#include "sherpa/version.hpp"

namespace {

enum class ExitCode {
  kSuccess = 0,
  kUsage = 2,
  kRepositoryUnavailable = 3,
  kInternalFailure = 10,
};

}  // namespace

int main(int argc, char** argv) {
  CLI::App app{"Evidence-backed codebase intelligence", "sherpa"};
  app.set_version_flag("--version", SHERPA_VERSION);
  app.require_subcommand(1);

  std::filesystem::path repository_path;
  std::filesystem::path database_path;
  auto* index_command = app.add_subcommand("index", "Index C and C++ files in a repository");
  index_command->add_option("repo", repository_path, "Repository path")->required();
  index_command->add_option("--database", database_path, "Explicit SQLite database path");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& error) {
    const int reported_exit_code = app.exit(error);
    if (reported_exit_code == static_cast<int>(CLI::ExitCodes::Success)) {
      return static_cast<int>(ExitCode::kSuccess);
    }
    return static_cast<int>(ExitCode::kUsage);
  }

  if (*index_command) {
    try {
      sherpa::IndexService service;
      const auto result = service.index({
          .repository_path = repository_path,
          .database_path = database_path,
      });

      std::cout << "Indexed " << result.indexed_files << " C/C++ files\n"
                << "Symbols: " << result.extracted_symbols << '\n'
                << "Includes: " << result.extracted_includes << '\n'
                << "Diagnostics: " << result.diagnostics << '\n'
                << "Relationships: " << result.relationships << " ("
                << result.resolved_relationships << " resolved, " << result.ambiguous_relationships
                << " ambiguous, " << result.unresolved_relationships << " unresolved)\n"
                << "Repository: " << result.repository_path.generic_string() << '\n'
                << "Database: " << result.database_path.generic_string() << '\n';
      for (const auto& warning : result.warnings) {
        std::cerr << "warning: " << warning << '\n';
      }
      return static_cast<int>(ExitCode::kSuccess);
    } catch (const std::invalid_argument& error) {
      std::cerr << "error: " << error.what() << '\n';
      return static_cast<int>(ExitCode::kRepositoryUnavailable);
    } catch (const std::exception& error) {
      std::cerr << "error: indexing failed: " << error.what() << '\n';
      return static_cast<int>(ExitCode::kInternalFailure);
    }
  }

  return static_cast<int>(ExitCode::kUsage);
}
