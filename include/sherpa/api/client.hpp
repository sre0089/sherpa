#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "sherpa/api/plugin.hpp"
#include "sherpa/domain/call_query.hpp"
#include "sherpa/domain/file_query.hpp"
#include "sherpa/domain/graph_export.hpp"
#include "sherpa/domain/impact.hpp"
#include "sherpa/domain/path_query.hpp"
#include "sherpa/domain/symbol_query.hpp"

namespace sherpa::api {

inline constexpr std::uint32_t kApiVersion = 1;

enum class ErrorCode {
  kInvalidArgument,
  kRepositoryUnavailable,
  kIndexUnavailable,
  kNotFound,
  kAmbiguousSymbol,
  kPluginFailure,
  kInternalFailure,
};

class Error : public std::runtime_error {
 public:
  Error(ErrorCode code,
        std::string message,
        std::vector<QuerySymbol> candidates = {},
        std::string plugin_id = {},
        bool operation_completed = false);

  [[nodiscard]] ErrorCode code() const noexcept;
  [[nodiscard]] const std::vector<QuerySymbol>& candidates() const noexcept;
  [[nodiscard]] const std::string& plugin_id() const noexcept;
  [[nodiscard]] bool operation_completed() const noexcept;

 private:
  ErrorCode code_;
  std::vector<QuerySymbol> candidates_;
  std::string plugin_id_;
  bool operation_completed_;
};

struct Repository {
  std::filesystem::path path{"."};
  std::filesystem::path database_path;
};

struct SymbolSelector {
  std::string name;
  std::string signature;
  std::string file_path;
};

struct IndexRequest {
  std::filesystem::path repository_path;
  std::filesystem::path database_path;
};

struct IndexResult {
  std::filesystem::path repository_path;
  std::filesystem::path database_path;
  std::size_t indexed_files{};
  std::size_t added_files{};
  std::size_t modified_files{};
  std::size_t unchanged_files{};
  std::size_t deleted_files{};
  std::size_t parsed_files{};
  std::size_t extracted_symbols{};
  std::size_t extracted_includes{};
  std::size_t diagnostics{};
  std::size_t relationships{};
  std::size_t resolved_relationships{};
  std::size_t ambiguous_relationships{};
  std::size_t unresolved_relationships{};
  std::uint64_t scan_milliseconds{};
  std::uint64_t cache_load_milliseconds{};
  std::uint64_t parse_milliseconds{};
  std::uint64_t relationship_milliseconds{};
  std::uint64_t persistence_milliseconds{};
  std::uint64_t total_milliseconds{};
  std::vector<std::string> warnings;
};

struct SymbolRequest {
  Repository repository;
  SymbolSelector symbol;
};

struct FileRequest {
  Repository repository;
  std::string path;
};

struct ImpactRequest {
  Repository repository;
  std::string target;
  std::string signature;
  std::string file_path;
};

struct PathRequest {
  Repository repository;
  SymbolSelector source;
  SymbolSelector target;
};

class Client {
 public:
  explicit Client(PluginRegistry plugins = {});

  [[nodiscard]] IndexResult index(const IndexRequest& request) const;
  [[nodiscard]] SymbolQueryResult symbol(const SymbolRequest& request) const;
  [[nodiscard]] FileQueryResult file(const FileRequest& request) const;
  [[nodiscard]] CallQueryResult callers(const SymbolRequest& request) const;
  [[nodiscard]] CallQueryResult callees(const SymbolRequest& request) const;
  [[nodiscard]] ImpactResult impact(const ImpactRequest& request) const;
  [[nodiscard]] PathResult path(const PathRequest& request) const;
  [[nodiscard]] GraphExport graph(const Repository& repository) const;

 private:
  PluginRegistry plugins_;
};

[[nodiscard]] const char* to_string(ErrorCode code) noexcept;

}  // namespace sherpa::api
