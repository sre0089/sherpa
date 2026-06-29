#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "sherpa/application/call_query_service.hpp"
#include "sherpa/application/impact_service.hpp"
#include "sherpa/application/index_service.hpp"
#include "sherpa/application/path_service.hpp"
#include "sherpa/presentation/call_query_renderer.hpp"
#include "sherpa/presentation/impact_renderer.hpp"
#include "sherpa/presentation/path_renderer.hpp"
#include "sherpa/version.hpp"

namespace {

enum class ExitCode {
  kSuccess = 0,
  kUsage = 2,
  kRepositoryUnavailable = 3,
  kIndexUnavailable = 4,
  kSymbolNotFound = 5,
  kAmbiguousSymbol = 6,
  kInternalFailure = 10,
};

int run_query(sherpa::CallQueryDirection direction, const std::string& symbol,
              const std::filesystem::path& repository_path,
              const std::filesystem::path& database_path, const std::string& format) {
  try {
    const auto result = sherpa::CallQueryService{}.query({
        .direction = direction,
        .symbol = symbol,
        .repository_path = repository_path,
        .database_path = database_path,
    });
    if (format == "json") {
      sherpa::write_call_query_json(std::cout, result);
    } else {
      sherpa::write_call_query_text(std::cout, result);
    }
    return static_cast<int>(ExitCode::kSuccess);
  } catch (const sherpa::IndexUnavailableError& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kIndexUnavailable);
  } catch (const sherpa::SymbolNotFoundError& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kSymbolNotFound);
  } catch (const sherpa::AmbiguousSymbolError& error) {
    std::cerr << "error: " << error.what() << '\n';
    for (const auto& candidate : error.candidates()) {
      std::cerr << "  " << candidate.qualified_name << " (" << candidate.signature << ") at "
                << candidate.file_path << ':' << candidate.range.start_line << '\n';
    }
    return static_cast<int>(ExitCode::kAmbiguousSymbol);
  } catch (const std::invalid_argument& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kRepositoryUnavailable);
  } catch (const std::exception& error) {
    std::cerr << "error: query failed: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kInternalFailure);
  }
}

int run_impact(const std::string& target, const std::filesystem::path& repository_path,
               const std::filesystem::path& database_path, const std::string& format) {
  try {
    const auto result = sherpa::ImpactService{}.analyze({
        .target = target,
        .repository_path = repository_path,
        .database_path = database_path,
    });
    if (format == "json") {
      sherpa::write_impact_json(std::cout, result);
    } else {
      sherpa::write_impact_text(std::cout, result);
    }
    return static_cast<int>(ExitCode::kSuccess);
  } catch (const sherpa::IndexUnavailableError& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kIndexUnavailable);
  } catch (const sherpa::ImpactTargetNotFoundError& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kSymbolNotFound);
  } catch (const sherpa::AmbiguousSymbolError& error) {
    std::cerr << "error: " << error.what() << '\n';
    for (const auto& candidate : error.candidates()) {
      std::cerr << "  " << candidate.qualified_name << " (" << candidate.signature << ") at "
                << candidate.file_path << ':' << candidate.range.start_line << '\n';
    }
    return static_cast<int>(ExitCode::kAmbiguousSymbol);
  } catch (const std::invalid_argument& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kRepositoryUnavailable);
  } catch (const std::exception& error) {
    std::cerr << "error: impact analysis failed: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kInternalFailure);
  }
}

int run_path(const std::string& source, const std::string& target,
             const std::filesystem::path& repository_path,
             const std::filesystem::path& database_path, const std::string& format) {
  try {
    const auto result = sherpa::PathService{}.find({
        .source = source,
        .target = target,
        .repository_path = repository_path,
        .database_path = database_path,
    });
    if (format == "json") {
      sherpa::write_path_json(std::cout, result);
    } else {
      sherpa::write_path_text(std::cout, result);
    }
    return static_cast<int>(ExitCode::kSuccess);
  } catch (const sherpa::IndexUnavailableError& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kIndexUnavailable);
  } catch (const sherpa::SymbolNotFoundError& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kSymbolNotFound);
  } catch (const sherpa::AmbiguousSymbolError& error) {
    std::cerr << "error: " << error.what() << '\n';
    for (const auto& candidate : error.candidates()) {
      std::cerr << "  " << candidate.qualified_name << " (" << candidate.signature << ") at "
                << candidate.file_path << ':' << candidate.range.start_line << '\n';
    }
    return static_cast<int>(ExitCode::kAmbiguousSymbol);
  } catch (const std::invalid_argument& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kRepositoryUnavailable);
  } catch (const std::exception& error) {
    std::cerr << "error: path query failed: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kInternalFailure);
  }
}

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

  std::string callers_symbol;
  std::filesystem::path callers_repository{"."};
  std::filesystem::path callers_database;
  std::string callers_format{"text"};
  auto* callers_command = app.add_subcommand("callers", "Find functions that call a symbol");
  callers_command->add_option("symbol", callers_symbol, "Qualified or unqualified symbol name")
      ->required();
  callers_command->add_option("--repo", callers_repository, "Indexed repository path");
  callers_command->add_option("--database", callers_database, "Explicit SQLite database path");
  callers_command->add_option("--format", callers_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));

  std::string callees_symbol;
  std::filesystem::path callees_repository{"."};
  std::filesystem::path callees_database;
  std::string callees_format{"text"};
  auto* callees_command = app.add_subcommand("callees", "Find functions called by a symbol");
  callees_command->add_option("symbol", callees_symbol, "Qualified or unqualified symbol name")
      ->required();
  callees_command->add_option("--repo", callees_repository, "Indexed repository path");
  callees_command->add_option("--database", callees_database, "Explicit SQLite database path");
  callees_command->add_option("--format", callees_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));

  std::string impact_target;
  std::filesystem::path impact_repository{"."};
  std::filesystem::path impact_database;
  std::string impact_format{"text"};
  auto* impact_command =
      app.add_subcommand("impact", "Find files and functions affected by a change");
  impact_command->add_option("target", impact_target, "Repository-relative file or symbol name")
      ->required();
  impact_command->add_option("--repo", impact_repository, "Indexed repository path");
  impact_command->add_option("--database", impact_database, "Explicit SQLite database path");
  impact_command->add_option("--format", impact_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));

  std::string path_source;
  std::string path_target;
  std::filesystem::path path_repository{"."};
  std::filesystem::path path_database;
  std::string path_format{"text"};
  auto* path_command = app.add_subcommand("path", "Find a directed call path between symbols");
  path_command->add_option("source", path_source, "Starting symbol")->required();
  path_command->add_option("target", path_target, "Destination symbol")->required();
  path_command->add_option("--repo", path_repository, "Indexed repository path");
  path_command->add_option("--database", path_database, "Explicit SQLite database path");
  path_command->add_option("--format", path_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));

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

  if (*callers_command) {
    return run_query(sherpa::CallQueryDirection::kCallers, callers_symbol, callers_repository,
                     callers_database, callers_format);
  }
  if (*callees_command) {
    return run_query(sherpa::CallQueryDirection::kCallees, callees_symbol, callees_repository,
                     callees_database, callees_format);
  }
  if (*impact_command) {
    return run_impact(impact_target, impact_repository, impact_database, impact_format);
  }
  if (*path_command) {
    return run_path(path_source, path_target, path_repository, path_database, path_format);
  }

  return static_cast<int>(ExitCode::kUsage);
}
