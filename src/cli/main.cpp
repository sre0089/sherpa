#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "graph_export_command.hpp"
#include "sherpa/application/call_query_service.hpp"
#include "sherpa/application/file_query_service.hpp"
#include "sherpa/application/impact_service.hpp"
#include "sherpa/application/index_service.hpp"
#include "sherpa/application/path_service.hpp"
#include "sherpa/application/symbol_query_service.hpp"
#include "sherpa/presentation/call_query_renderer.hpp"
#include "sherpa/presentation/file_query_renderer.hpp"
#include "sherpa/presentation/impact_renderer.hpp"
#include "sherpa/presentation/path_renderer.hpp"
#include "sherpa/presentation/query_error_renderer.hpp"
#include "sherpa/presentation/symbol_query_renderer.hpp"
#include "sherpa/version.hpp"

namespace {

enum class ExitCode {
  kSuccess = 0,
  kUsage = 2,
  kRepositoryUnavailable = 3,
  kIndexUnavailable = 4,
  kSymbolNotFound = 5,
  kAmbiguousSymbol = 6,
  kOutputUnavailable = 7,
  kInternalFailure = 10,
};

void write_query_error(const std::string& format, std::string_view code,
                       std::string_view message,
                       const std::vector<sherpa::QuerySymbol>& candidates = {}) {
  if (format == "json") {
    sherpa::write_query_error_json(std::cout, code, message, candidates);
    return;
  }

  std::cerr << "error: " << message << '\n';
  for (const auto& candidate : candidates) {
    std::cerr << "  " << candidate.qualified_name << " (" << candidate.signature << ") at "
              << candidate.file_path << ':' << candidate.range.start_line << '\n';
  }
}

bool requests_json_output(int argc, char** argv) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--format=json") {
      return true;
    }
    if (argument == "--format" && index + 1 < argc &&
        std::string_view{argv[index + 1]} == "json") {
      return true;
    }
  }
  return false;
}

int run_query(sherpa::CallQueryDirection direction, const std::string& symbol,
              const std::string& signature, const std::string& file_path,
              const std::filesystem::path& repository_path,
              const std::filesystem::path& database_path, const std::string& format) {
  try {
    const auto result = sherpa::CallQueryService{}.query({
        .direction = direction,
        .symbol = symbol,
        .signature = signature,
        .file_path = file_path,
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
    write_query_error(format, "index_unavailable", error.what());
    return static_cast<int>(ExitCode::kIndexUnavailable);
  } catch (const sherpa::SymbolNotFoundError& error) {
    write_query_error(format, "not_found", error.what());
    return static_cast<int>(ExitCode::kSymbolNotFound);
  } catch (const sherpa::AmbiguousSymbolError& error) {
    write_query_error(format, "ambiguous_symbol", error.what(), error.candidates());
    return static_cast<int>(ExitCode::kAmbiguousSymbol);
  } catch (const std::invalid_argument& error) {
    write_query_error(format, "repository_unavailable", error.what());
    return static_cast<int>(ExitCode::kRepositoryUnavailable);
  } catch (const std::exception& error) {
    write_query_error(format, "internal_failure",
                      std::string{"query failed: "} + error.what());
    return static_cast<int>(ExitCode::kInternalFailure);
  }
}

int run_symbol_query(const std::string& symbol, const std::string& signature,
                     const std::string& file_path,
                     const std::filesystem::path& repository_path,
                     const std::filesystem::path& database_path, const std::string& format) {
  try {
    const auto result = sherpa::SymbolQueryService{}.query({
        .symbol = symbol,
        .signature = signature,
        .file_path = file_path,
        .repository_path = repository_path,
        .database_path = database_path,
    });
    if (format == "json") {
      sherpa::write_symbol_query_json(std::cout, result);
    } else {
      sherpa::write_symbol_query_text(std::cout, result);
    }
    return static_cast<int>(ExitCode::kSuccess);
  } catch (const sherpa::IndexUnavailableError& error) {
    write_query_error(format, "index_unavailable", error.what());
    return static_cast<int>(ExitCode::kIndexUnavailable);
  } catch (const sherpa::SymbolNotFoundError& error) {
    write_query_error(format, "not_found", error.what());
    return static_cast<int>(ExitCode::kSymbolNotFound);
  } catch (const sherpa::AmbiguousSymbolError& error) {
    write_query_error(format, "ambiguous_symbol", error.what(), error.candidates());
    return static_cast<int>(ExitCode::kAmbiguousSymbol);
  } catch (const std::invalid_argument& error) {
    write_query_error(format, "repository_unavailable", error.what());
    return static_cast<int>(ExitCode::kRepositoryUnavailable);
  } catch (const std::exception& error) {
    write_query_error(format, "internal_failure",
                      std::string{"symbol query failed: "} + error.what());
    return static_cast<int>(ExitCode::kInternalFailure);
  }
}

int run_file_query(const std::string& path, const std::filesystem::path& repository_path,
                   const std::filesystem::path& database_path, const std::string& format) {
  try {
    const auto result = sherpa::FileQueryService{}.query({
        .path = path,
        .repository_path = repository_path,
        .database_path = database_path,
    });
    if (format == "json") {
      sherpa::write_file_query_json(std::cout, result);
    } else {
      sherpa::write_file_query_text(std::cout, result);
    }
    return static_cast<int>(ExitCode::kSuccess);
  } catch (const sherpa::IndexUnavailableError& error) {
    write_query_error(format, "index_unavailable", error.what());
    return static_cast<int>(ExitCode::kIndexUnavailable);
  } catch (const sherpa::FileNotFoundError& error) {
    write_query_error(format, "not_found", error.what());
    return static_cast<int>(ExitCode::kSymbolNotFound);
  } catch (const std::invalid_argument& error) {
    write_query_error(format, "repository_unavailable", error.what());
    return static_cast<int>(ExitCode::kRepositoryUnavailable);
  } catch (const std::exception& error) {
    write_query_error(format, "internal_failure",
                      std::string{"file query failed: "} + error.what());
    return static_cast<int>(ExitCode::kInternalFailure);
  }
}

int run_impact(const std::string& target, const std::string& signature,
               const std::string& file_path, const std::filesystem::path& repository_path,
               const std::filesystem::path& database_path, const std::string& format) {
  try {
    const auto result = sherpa::ImpactService{}.analyze({
        .target = target,
        .signature = signature,
        .file_path = file_path,
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
    write_query_error(format, "index_unavailable", error.what());
    return static_cast<int>(ExitCode::kIndexUnavailable);
  } catch (const sherpa::ImpactTargetNotFoundError& error) {
    write_query_error(format, "not_found", error.what());
    return static_cast<int>(ExitCode::kSymbolNotFound);
  } catch (const sherpa::AmbiguousSymbolError& error) {
    write_query_error(format, "ambiguous_symbol", error.what(), error.candidates());
    return static_cast<int>(ExitCode::kAmbiguousSymbol);
  } catch (const std::invalid_argument& error) {
    write_query_error(format, "repository_unavailable", error.what());
    return static_cast<int>(ExitCode::kRepositoryUnavailable);
  } catch (const std::exception& error) {
    write_query_error(format, "internal_failure",
                      std::string{"impact analysis failed: "} + error.what());
    return static_cast<int>(ExitCode::kInternalFailure);
  }
}

int run_path(const std::string& source, const std::string& target,
             const std::string& source_signature, const std::string& source_file_path,
             const std::string& target_signature, const std::string& target_file_path,
             const std::filesystem::path& repository_path,
             const std::filesystem::path& database_path, const std::string& format) {
  try {
    const auto result = sherpa::PathService{}.find({
        .source = source,
        .target = target,
        .source_signature = source_signature,
        .source_file_path = source_file_path,
        .target_signature = target_signature,
        .target_file_path = target_file_path,
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
    write_query_error(format, "index_unavailable", error.what());
    return static_cast<int>(ExitCode::kIndexUnavailable);
  } catch (const sherpa::SymbolNotFoundError& error) {
    write_query_error(format, "not_found", error.what());
    return static_cast<int>(ExitCode::kSymbolNotFound);
  } catch (const sherpa::AmbiguousSymbolError& error) {
    write_query_error(format, "ambiguous_symbol", error.what(), error.candidates());
    return static_cast<int>(ExitCode::kAmbiguousSymbol);
  } catch (const std::invalid_argument& error) {
    write_query_error(format, "repository_unavailable", error.what());
    return static_cast<int>(ExitCode::kRepositoryUnavailable);
  } catch (const std::exception& error) {
    write_query_error(format, "internal_failure",
                      std::string{"path query failed: "} + error.what());
    return static_cast<int>(ExitCode::kInternalFailure);
  }
}

int run_export(const std::filesystem::path& output_path,
               const std::filesystem::path& repository_path,
               const std::filesystem::path& database_path, bool force) {
  try {
    const auto result = sherpa::cli::export_graph({
        .output_path = output_path,
        .repository_path = repository_path,
        .database_path = database_path,
        .force = force,
    });
    std::cout << "Exported " << result.nodes << " nodes and " << result.edges << " edges\n"
              << "Output: " << result.output_path.generic_string() << '\n';
    return static_cast<int>(ExitCode::kSuccess);
  } catch (const sherpa::IndexUnavailableError& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kIndexUnavailable);
  } catch (const sherpa::cli::ExportOutputError& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kOutputUnavailable);
  } catch (const std::invalid_argument& error) {
    std::cerr << "error: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kRepositoryUnavailable);
  } catch (const std::exception& error) {
    std::cerr << "error: graph export failed: " << error.what() << '\n';
    return static_cast<int>(ExitCode::kInternalFailure);
  }
}

}  // namespace

int main(int argc, char** argv) {
  const bool json_output_requested = requests_json_output(argc, argv);
  CLI::App app{"Evidence-backed codebase intelligence", "sherpa"};
  app.set_version_flag("--version", SHERPA_VERSION);
  app.require_subcommand(1);

  std::filesystem::path repository_path;
  std::filesystem::path database_path;
  auto* index_command = app.add_subcommand("index", "Index C and C++ files in a repository");
  index_command->add_option("repo", repository_path, "Repository path")->required();
  index_command->add_option("--database", database_path, "Explicit SQLite database path");

  auto* query_command = app.add_subcommand("query", "Run read-only graph queries");
  query_command->require_subcommand(1);

  std::string callers_symbol;
  std::string callers_signature;
  std::string callers_file;
  std::filesystem::path callers_repository{"."};
  std::filesystem::path callers_database;
  std::string callers_format{"text"};
  auto* callers_command = app.add_subcommand("callers", "Find functions that call a symbol");
  callers_command->add_option("symbol", callers_symbol, "Qualified or unqualified symbol name")
      ->required();
  callers_command->add_option("--signature", callers_signature, "Exact displayed signature");
  callers_command->add_option("--file", callers_file, "File containing the definition");
  callers_command->add_option("--repo", callers_repository, "Indexed repository path");
  callers_command->add_option("--database", callers_database, "Explicit SQLite database path");
  callers_command->add_option("--format", callers_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));
  auto* query_callers_command =
      query_command->add_subcommand("callers", "Find functions that call a symbol");
  query_callers_command->add_option("symbol", callers_symbol,
                                    "Qualified or unqualified symbol name")
      ->required();
  query_callers_command->add_option("--signature", callers_signature, "Exact displayed signature");
  query_callers_command->add_option("--file", callers_file, "File containing the definition");
  query_callers_command->add_option("--repo", callers_repository, "Indexed repository path");
  query_callers_command->add_option("--database", callers_database, "Explicit SQLite database path");
  query_callers_command->add_option("--format", callers_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));

  std::string callees_symbol;
  std::string callees_signature;
  std::string callees_file;
  std::filesystem::path callees_repository{"."};
  std::filesystem::path callees_database;
  std::string callees_format{"text"};
  auto* callees_command = app.add_subcommand("callees", "Find functions called by a symbol");
  callees_command->add_option("symbol", callees_symbol, "Qualified or unqualified symbol name")
      ->required();
  callees_command->add_option("--signature", callees_signature, "Exact displayed signature");
  callees_command->add_option("--file", callees_file, "File containing the definition");
  callees_command->add_option("--repo", callees_repository, "Indexed repository path");
  callees_command->add_option("--database", callees_database, "Explicit SQLite database path");
  callees_command->add_option("--format", callees_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));
  auto* query_callees_command =
      query_command->add_subcommand("callees", "Find functions called by a symbol");
  query_callees_command->add_option("symbol", callees_symbol,
                                    "Qualified or unqualified symbol name")
      ->required();
  query_callees_command->add_option("--signature", callees_signature, "Exact displayed signature");
  query_callees_command->add_option("--file", callees_file, "File containing the definition");
  query_callees_command->add_option("--repo", callees_repository, "Indexed repository path");
  query_callees_command->add_option("--database", callees_database, "Explicit SQLite database path");
  query_callees_command->add_option("--format", callees_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));

  std::string query_symbol_name;
  std::string query_symbol_signature;
  std::string query_symbol_file;
  std::filesystem::path query_symbol_repository{"."};
  std::filesystem::path query_symbol_database;
  std::string query_symbol_format{"text"};
  auto* query_symbol_command =
      query_command->add_subcommand("symbol", "Show symbol metadata and direct call counts");
  query_symbol_command->add_option("symbol", query_symbol_name,
                                   "Qualified or unqualified symbol name")
      ->required();
  query_symbol_command->add_option("--signature", query_symbol_signature,
                                   "Exact displayed signature");
  query_symbol_command->add_option("--file", query_symbol_file,
                                   "File containing the definition");
  query_symbol_command->add_option("--repo", query_symbol_repository, "Indexed repository path");
  query_symbol_command->add_option("--database", query_symbol_database,
                                   "Explicit SQLite database path");
  query_symbol_command->add_option("--format", query_symbol_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));

  std::string query_file_path;
  std::filesystem::path query_file_repository{"."};
  std::filesystem::path query_file_database;
  std::string query_file_format{"text"};
  auto* query_file_command =
      query_command->add_subcommand("file", "Show file definitions and direct include edges");
  query_file_command->add_option("path", query_file_path, "Repository-relative file path")
      ->required();
  query_file_command->add_option("--repo", query_file_repository, "Indexed repository path");
  query_file_command->add_option("--database", query_file_database,
                                 "Explicit SQLite database path");
  query_file_command->add_option("--format", query_file_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));

  std::string impact_target;
  std::string impact_signature;
  std::string impact_file;
  std::filesystem::path impact_repository{"."};
  std::filesystem::path impact_database;
  std::string impact_format{"text"};
  auto* impact_command =
      app.add_subcommand("impact", "Find files and functions affected by a change");
  impact_command->add_option("target", impact_target, "Repository-relative file or symbol name")
      ->required();
  impact_command->add_option("--signature", impact_signature, "Exact displayed symbol signature");
  impact_command->add_option("--file", impact_file, "File containing the symbol definition");
  impact_command->add_option("--repo", impact_repository, "Indexed repository path");
  impact_command->add_option("--database", impact_database, "Explicit SQLite database path");
  impact_command->add_option("--format", impact_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));

  std::string path_source;
  std::string path_target;
  std::string path_source_signature;
  std::string path_source_file;
  std::string path_target_signature;
  std::string path_target_file;
  std::filesystem::path path_repository{"."};
  std::filesystem::path path_database;
  std::string path_format{"text"};
  auto* path_command = app.add_subcommand("path", "Find a directed call path between symbols");
  path_command->add_option("source", path_source, "Starting symbol")->required();
  path_command->add_option("target", path_target, "Destination symbol")->required();
  path_command->add_option("--source-signature", path_source_signature,
                           "Exact displayed source signature");
  path_command->add_option("--source-file", path_source_file,
                           "File containing the source definition");
  path_command->add_option("--target-signature", path_target_signature,
                           "Exact displayed target signature");
  path_command->add_option("--target-file", path_target_file,
                           "File containing the target definition");
  path_command->add_option("--repo", path_repository, "Indexed repository path");
  path_command->add_option("--database", path_database, "Explicit SQLite database path");
  path_command->add_option("--format", path_format, "Output format: text or json")
      ->check(CLI::IsMember({"text", "json"}));

  std::filesystem::path export_output;
  std::filesystem::path export_repository{"."};
  std::filesystem::path export_database;
  bool export_force = false;
  auto* export_command = app.add_subcommand("export", "Export the indexed graph as JSON");
  export_command->add_option("output", export_output, "Output JSON file")->required();
  export_command->add_option("--repo", export_repository, "Indexed repository path");
  export_command->add_option("--database", export_database, "Explicit SQLite database path");
  export_command->add_flag("--force", export_force, "Replace an existing regular file");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& error) {
    if (error.get_exit_code() == static_cast<int>(CLI::ExitCodes::Success)) {
      static_cast<void>(app.exit(error));
      return static_cast<int>(ExitCode::kSuccess);
    }
    if (json_output_requested) {
      sherpa::write_query_error_json(std::cout, "invalid_usage", error.what());
    } else {
      static_cast<void>(app.exit(error));
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
    return run_query(sherpa::CallQueryDirection::kCallers, callers_symbol, callers_signature,
                     callers_file, callers_repository, callers_database, callers_format);
  }
  if (*query_callers_command) {
    return run_query(sherpa::CallQueryDirection::kCallers, callers_symbol, callers_signature,
                     callers_file, callers_repository, callers_database, callers_format);
  }
  if (*callees_command) {
    return run_query(sherpa::CallQueryDirection::kCallees, callees_symbol, callees_signature,
                     callees_file, callees_repository, callees_database, callees_format);
  }
  if (*query_callees_command) {
    return run_query(sherpa::CallQueryDirection::kCallees, callees_symbol, callees_signature,
                     callees_file, callees_repository, callees_database, callees_format);
  }
  if (*query_symbol_command) {
    return run_symbol_query(query_symbol_name, query_symbol_signature, query_symbol_file,
                            query_symbol_repository, query_symbol_database, query_symbol_format);
  }
  if (*query_file_command) {
    return run_file_query(query_file_path, query_file_repository, query_file_database,
                          query_file_format);
  }
  if (*impact_command) {
    return run_impact(impact_target, impact_signature, impact_file, impact_repository,
                      impact_database, impact_format);
  }
  if (*path_command) {
    return run_path(path_source, path_target, path_source_signature, path_source_file,
                    path_target_signature, path_target_file, path_repository, path_database,
                    path_format);
  }
  if (*export_command) {
    return run_export(export_output, export_repository, export_database, export_force);
  }

  return static_cast<int>(ExitCode::kUsage);
}
