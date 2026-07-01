#include "sherpa/api/client.hpp"

#include <exception>
#include <functional>
#include <utility>

#include "sherpa/application/call_query_service.hpp"
#include "sherpa/application/file_query_service.hpp"
#include "sherpa/application/graph_export_service.hpp"
#include "sherpa/application/impact_service.hpp"
#include "sherpa/application/index_service.hpp"
#include "sherpa/application/path_service.hpp"
#include "sherpa/application/query_errors.hpp"
#include "sherpa/application/symbol_query_service.hpp"

namespace sherpa::api {
namespace {

template <typename Result, typename Function>
Result invoke(Function&& function) {
  try {
    return std::invoke(std::forward<Function>(function));
  } catch (const Error&) {
    throw;
  } catch (const IndexUnavailableError& error) {
    throw Error(ErrorCode::kIndexUnavailable, error.what());
  } catch (const SymbolNotFoundError& error) {
    throw Error(ErrorCode::kNotFound, error.what());
  } catch (const FileNotFoundError& error) {
    throw Error(ErrorCode::kNotFound, error.what());
  } catch (const ImpactTargetNotFoundError& error) {
    throw Error(ErrorCode::kNotFound, error.what());
  } catch (const AmbiguousSymbolError& error) {
    throw Error(ErrorCode::kAmbiguousSymbol, error.what(), error.candidates());
  } catch (const std::invalid_argument& error) {
    throw Error(ErrorCode::kRepositoryUnavailable, error.what());
  } catch (const std::exception& error) {
    throw Error(ErrorCode::kInternalFailure, error.what());
  }
}

void require(bool condition, const char* message) {
  if (!condition) {
    throw Error(ErrorCode::kInvalidArgument, message);
  }
}

CallQueryResult query_calls(const SymbolRequest& request, CallQueryDirection direction) {
  return CallQueryService{}.query({
      .direction = direction,
      .symbol = request.symbol.name,
      .signature = request.symbol.signature,
      .file_path = request.symbol.file_path,
      .repository_path = request.repository.path,
      .database_path = request.repository.database_path,
  });
}

[[noreturn]] void throw_plugin_failure(const RegisteredPlugin& plugin,
                                       const char* phase,
                                       bool operation_completed) {
  try {
    throw;
  } catch (const std::exception& error) {
    throw Error(ErrorCode::kPluginFailure,
                "plugin '" + plugin.descriptor.id + "' failed " + phase + ": " + error.what(),
                {},
                plugin.descriptor.id,
                operation_completed);
  } catch (...) {
    throw Error(ErrorCode::kPluginFailure,
                "plugin '" + plugin.descriptor.id + "' failed " + phase,
                {},
                plugin.descriptor.id,
                operation_completed);
  }
}

template <typename Result, typename Function>
Result invoke_with_plugins(const PluginRegistry& registry,
                           const OperationContext& context,
                           Function&& function) {
  for (const auto& plugin : registry.plugins()) {
    try {
      plugin.instance->before_operation(context);
    } catch (...) {
      throw_plugin_failure(plugin, "before operation", false);
    }
  }

  auto result = invoke<Result>(std::forward<Function>(function));

  for (const auto& plugin : registry.plugins()) {
    try {
      plugin.instance->after_operation(context);
    } catch (...) {
      throw_plugin_failure(plugin, "after operation", true);
    }
  }

  return result;
}

}  // namespace

Error::Error(ErrorCode code,
             std::string message,
             std::vector<QuerySymbol> candidates,
             std::string plugin_id,
             bool operation_completed)
    : std::runtime_error(std::move(message)),
      code_(code),
      candidates_(std::move(candidates)),
      plugin_id_(std::move(plugin_id)),
      operation_completed_(operation_completed) {}

ErrorCode Error::code() const noexcept { return code_; }

const std::vector<QuerySymbol>& Error::candidates() const noexcept { return candidates_; }

const std::string& Error::plugin_id() const noexcept { return plugin_id_; }

bool Error::operation_completed() const noexcept { return operation_completed_; }

Client::Client(PluginRegistry plugins) : plugins_(std::move(plugins)) {}

IndexResult Client::index(const IndexRequest& request) const {
  require(!request.repository_path.empty(), "repository path is required");
  return invoke_with_plugins<IndexResult>(
      plugins_,
      {
          .operation = Operation::kIndex,
          .repository_path = request.repository_path,
          .database_path = request.database_path,
      },
      [&request] {
        const auto result = IndexService{}.index({
            .repository_path = request.repository_path,
            .database_path = request.database_path,
        });
        return IndexResult{
            .repository_path = result.repository_path,
            .database_path = result.database_path,
            .indexed_files = result.indexed_files,
            .added_files = result.added_files,
            .modified_files = result.modified_files,
            .unchanged_files = result.unchanged_files,
            .deleted_files = result.deleted_files,
            .parsed_files = result.parsed_files,
            .extracted_symbols = result.extracted_symbols,
            .extracted_includes = result.extracted_includes,
            .diagnostics = result.diagnostics,
            .relationships = result.relationships,
            .resolved_relationships = result.resolved_relationships,
            .ambiguous_relationships = result.ambiguous_relationships,
            .unresolved_relationships = result.unresolved_relationships,
            .scan_milliseconds = result.scan_milliseconds,
            .cache_load_milliseconds = result.cache_load_milliseconds,
            .parse_milliseconds = result.parse_milliseconds,
            .relationship_milliseconds = result.relationship_milliseconds,
            .persistence_milliseconds = result.persistence_milliseconds,
            .total_milliseconds = result.total_milliseconds,
            .warnings = result.warnings,
        };
      });
}

SymbolQueryResult Client::symbol(const SymbolRequest& request) const {
  require(!request.repository.path.empty(), "repository path is required");
  require(!request.symbol.name.empty(), "symbol is required");
  return invoke_with_plugins<SymbolQueryResult>(
      plugins_,
      {
          .operation = Operation::kSymbol,
          .repository_path = request.repository.path,
          .database_path = request.repository.database_path,
      },
      [&request] {
        return SymbolQueryService{}.query({
            .symbol = request.symbol.name,
            .signature = request.symbol.signature,
            .file_path = request.symbol.file_path,
            .repository_path = request.repository.path,
            .database_path = request.repository.database_path,
        });
      });
}

FileQueryResult Client::file(const FileRequest& request) const {
  require(!request.repository.path.empty(), "repository path is required");
  require(!request.path.empty(), "file path is required");
  return invoke_with_plugins<FileQueryResult>(
      plugins_,
      {
          .operation = Operation::kFile,
          .repository_path = request.repository.path,
          .database_path = request.repository.database_path,
      },
      [&request] {
        return FileQueryService{}.query({
            .path = request.path,
            .repository_path = request.repository.path,
            .database_path = request.repository.database_path,
        });
      });
}

CallQueryResult Client::callers(const SymbolRequest& request) const {
  require(!request.repository.path.empty(), "repository path is required");
  require(!request.symbol.name.empty(), "symbol is required");
  return invoke_with_plugins<CallQueryResult>(
      plugins_,
      {
          .operation = Operation::kCallers,
          .repository_path = request.repository.path,
          .database_path = request.repository.database_path,
      },
      [&request] { return query_calls(request, CallQueryDirection::kCallers); });
}

CallQueryResult Client::callees(const SymbolRequest& request) const {
  require(!request.repository.path.empty(), "repository path is required");
  require(!request.symbol.name.empty(), "symbol is required");
  return invoke_with_plugins<CallQueryResult>(
      plugins_,
      {
          .operation = Operation::kCallees,
          .repository_path = request.repository.path,
          .database_path = request.repository.database_path,
      },
      [&request] { return query_calls(request, CallQueryDirection::kCallees); });
}

ImpactResult Client::impact(const ImpactRequest& request) const {
  require(!request.repository.path.empty(), "repository path is required");
  require(!request.target.empty(), "impact target is required");
  return invoke_with_plugins<ImpactResult>(
      plugins_,
      {
          .operation = Operation::kImpact,
          .repository_path = request.repository.path,
          .database_path = request.repository.database_path,
      },
      [&request] {
        return ImpactService{}.analyze({
            .target = request.target,
            .signature = request.signature,
            .file_path = request.file_path,
            .repository_path = request.repository.path,
            .database_path = request.repository.database_path,
        });
      });
}

PathResult Client::path(const PathRequest& request) const {
  require(!request.repository.path.empty(), "repository path is required");
  require(!request.source.name.empty(), "source symbol is required");
  require(!request.target.name.empty(), "target symbol is required");
  return invoke_with_plugins<PathResult>(
      plugins_,
      {
          .operation = Operation::kPath,
          .repository_path = request.repository.path,
          .database_path = request.repository.database_path,
      },
      [&request] {
        return PathService{}.find({
            .source = request.source.name,
            .target = request.target.name,
            .source_signature = request.source.signature,
            .source_file_path = request.source.file_path,
            .target_signature = request.target.signature,
            .target_file_path = request.target.file_path,
            .repository_path = request.repository.path,
            .database_path = request.repository.database_path,
        });
      });
}

GraphExport Client::graph(const Repository& repository) const {
  require(!repository.path.empty(), "repository path is required");
  return invoke_with_plugins<GraphExport>(
      plugins_,
      {
          .operation = Operation::kGraph,
          .repository_path = repository.path,
          .database_path = repository.database_path,
      },
      [&repository] {
        return GraphExportService{}.build({
            .repository_path = repository.path,
            .database_path = repository.database_path,
        });
      });
}

const char* to_string(ErrorCode code) noexcept {
  switch (code) {
    case ErrorCode::kInvalidArgument:
      return "invalid_argument";
    case ErrorCode::kRepositoryUnavailable:
      return "repository_unavailable";
    case ErrorCode::kIndexUnavailable:
      return "index_unavailable";
    case ErrorCode::kNotFound:
      return "not_found";
    case ErrorCode::kAmbiguousSymbol:
      return "ambiguous_symbol";
    case ErrorCode::kPluginFailure:
      return "plugin_failure";
    case ErrorCode::kInternalFailure:
      return "internal_failure";
  }
  return "internal_failure";
}

}  // namespace sherpa::api
