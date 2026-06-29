#include "sherpa/storage/sqlite_database.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>

#include "sherpa/storage/schema_sql.hpp"

namespace sherpa {
namespace {

class Statement {
 public:
  Statement(sqlite3* database, const char* sql) {
    if (sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(database));
    }
  }

  ~Statement() { sqlite3_finalize(statement_); }

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  sqlite3_stmt* get() const { return statement_; }

 private:
  sqlite3_stmt* statement_{};
};

void check(sqlite3* database, int status) {
  if (status != SQLITE_OK && status != SQLITE_DONE && status != SQLITE_ROW) {
    throw std::runtime_error(sqlite3_errmsg(database));
  }
}

void bind_text(sqlite3* database, sqlite3_stmt* statement, int index, const std::string& value) {
  check(database, sqlite3_bind_text(statement, index, value.c_str(), static_cast<int>(value.size()),
                                    SQLITE_TRANSIENT));
}

void bind_range(sqlite3* database, sqlite3_stmt* statement, int first_index,
                const SourceRange& range) {
  check(database, sqlite3_bind_int64(statement, first_index, range.start_line));
  check(database, sqlite3_bind_int64(statement, first_index + 1, range.start_column));
  check(database, sqlite3_bind_int64(statement, first_index + 2, range.end_line));
  check(database, sqlite3_bind_int64(statement, first_index + 3, range.end_column));
  check(database, sqlite3_bind_int64(statement, first_index + 4, range.start_byte));
  check(database, sqlite3_bind_int64(statement, first_index + 5, range.end_byte));
}

std::int64_t repository_id(sqlite3* database, const std::string& canonical_path) {
  Statement statement(database, "SELECT id FROM repositories WHERE canonical_path = ?1");
  bind_text(database, statement.get(), 1, canonical_path);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    throw std::runtime_error("repository row was not created");
  }
  return sqlite3_column_int64(statement.get(), 0);
}

using SymbolDatabaseKey = std::tuple<std::string, std::string, SymbolRole, std::uint32_t>;

SymbolDatabaseKey symbol_key(const SymbolReference& reference) {
  return {reference.file_path, reference.qualified_name, reference.role, reference.start_byte};
}

std::int64_t require_file_id(const std::map<std::string, std::int64_t>& file_ids,
                             const std::string& path) {
  const auto found = file_ids.find(path);
  if (found == file_ids.end()) {
    throw std::runtime_error("relationship references unknown file: " + path);
  }
  return found->second;
}

std::int64_t require_symbol_id(const std::map<SymbolDatabaseKey, std::int64_t>& symbol_ids,
                               const SymbolReference& reference) {
  const auto found = symbol_ids.find(symbol_key(reference));
  if (found == symbol_ids.end()) {
    throw std::runtime_error("relationship references unknown symbol: " + reference.qualified_name);
  }
  return found->second;
}

void bind_optional_id(sqlite3* database, sqlite3_stmt* statement, int index,
                      const std::optional<std::int64_t>& value) {
  check(database,
        value ? sqlite3_bind_int64(statement, index, *value) : sqlite3_bind_null(statement, index));
}

std::string column_text(sqlite3_stmt* statement, int index) {
  const auto* value = sqlite3_column_text(statement, index);
  return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

SourceRange column_range(sqlite3_stmt* statement, int first_index) {
  return SourceRange{
      .start_line = static_cast<std::uint32_t>(sqlite3_column_int64(statement, first_index)),
      .start_column = static_cast<std::uint32_t>(sqlite3_column_int64(statement, first_index + 1)),
      .end_line = static_cast<std::uint32_t>(sqlite3_column_int64(statement, first_index + 2)),
      .end_column = static_cast<std::uint32_t>(sqlite3_column_int64(statement, first_index + 3)),
      .start_byte = static_cast<std::uint32_t>(sqlite3_column_int64(statement, first_index + 4)),
      .end_byte = static_cast<std::uint32_t>(sqlite3_column_int64(statement, first_index + 5)),
  };
}

QuerySymbol column_symbol(sqlite3_stmt* statement, int first_index) {
  return QuerySymbol{
      .kind = column_text(statement, first_index),
      .name = column_text(statement, first_index + 1),
      .qualified_name = column_text(statement, first_index + 2),
      .signature = column_text(statement, first_index + 3),
      .file_path = column_text(statement, first_index + 4),
      .range = column_range(statement, first_index + 5),
  };
}

ResolutionStatus resolution_status(std::string_view value) {
  if (value == "resolved") {
    return ResolutionStatus::kResolved;
  }
  if (value == "ambiguous") {
    return ResolutionStatus::kAmbiguous;
  }
  if (value == "unresolved") {
    return ResolutionStatus::kUnresolved;
  }
  throw std::runtime_error("database contains an invalid relationship resolution");
}

Confidence confidence(std::string_view value) {
  if (value == "high") {
    return Confidence::kHigh;
  }
  if (value == "medium") {
    return Confidence::kMedium;
  }
  if (value == "low") {
    return Confidence::kLow;
  }
  throw std::runtime_error("database contains an invalid relationship confidence");
}

}  // namespace

SqliteDatabase::SqliteDatabase(const std::filesystem::path& path, DatabaseOpenMode mode) {
  if (mode == DatabaseOpenMode::kReadWriteCreate && !path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  const auto path_string = path.string();
  const int flags = mode == DatabaseOpenMode::kReadOnly
                        ? SQLITE_OPEN_READONLY
                        : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
  if (sqlite3_open_v2(path_string.c_str(), &database_, flags, nullptr) != SQLITE_OK) {
    const std::string message =
        database_ == nullptr ? "could not open SQLite database" : sqlite3_errmsg(database_);
    sqlite3_close(database_);
    database_ = nullptr;
    throw std::runtime_error(message);
  }
  sqlite3_busy_timeout(database_, 5000);
}

SqliteDatabase::~SqliteDatabase() {
  if (database_ != nullptr) {
    sqlite3_close(database_);
  }
}

void SqliteDatabase::execute(const std::string& sql) {
  char* error_message = nullptr;
  const int status = sqlite3_exec(database_, sql.c_str(), nullptr, nullptr, &error_message);
  if (status != SQLITE_OK) {
    const std::string message =
        error_message == nullptr ? sqlite3_errmsg(database_) : error_message;
    sqlite3_free(error_message);
    throw std::runtime_error(message);
  }
}

int SqliteDatabase::schema_version() const {
  Statement statement(database_, "SELECT version FROM schema_metadata");
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    throw std::runtime_error("database schema version is missing");
  }
  return sqlite3_column_int(statement.get(), 0);
}

void SqliteDatabase::validate_schema() const {
  constexpr int kSupportedSchemaVersion = 3;
  const int version = schema_version();
  if (version != kSupportedSchemaVersion) {
    throw std::runtime_error("unsupported database schema version: " + std::to_string(version));
  }
}

void SqliteDatabase::initialize_schema() {
  execute(kInitialSchema);
  constexpr int kSupportedSchemaVersion = 3;
  int version = schema_version();

  const auto migrate = [this](const char* sql) {
    execute("BEGIN IMMEDIATE");
    try {
      execute(sql);
      execute("COMMIT");
    } catch (...) {
      try {
        execute("ROLLBACK");
      } catch (...) {
      }
      throw;
    }
  };

  if (version == 1) {
    migrate(kSyntaxSchemaMigration);
    version = schema_version();
  }
  if (version == 2) {
    migrate(kRelationshipSchemaMigration);
    version = schema_version();
  }
  if (version != kSupportedSchemaVersion) {
    throw std::runtime_error("unsupported database schema version: " + std::to_string(version));
  }
}

bool SqliteDatabase::has_completed_index(const std::string& canonical_repository_path) const {
  Statement statement(database_,
                      "SELECT 1 FROM repositories repository "
                      "JOIN index_runs run ON run.id = repository.active_run_id "
                      "WHERE repository.canonical_path = ?1 AND run.status = 'completed' LIMIT 1");
  bind_text(database_, statement.get(), 1, canonical_repository_path);
  return sqlite3_step(statement.get()) == SQLITE_ROW;
}

std::vector<StoredQuerySymbol> SqliteDatabase::find_definition_symbols(
    const std::string& canonical_repository_path, const std::string& query) const {
  const auto find = [&](const char* name_column) {
    const std::string sql =
        "SELECT symbol.id, symbol.kind, symbol.name, symbol.qualified_name, symbol.signature, "
        "file.relative_path, symbol.start_line, symbol.start_column, symbol.end_line, "
        "symbol.end_column, symbol.start_byte, symbol.end_byte "
        "FROM symbols symbol "
        "JOIN files file ON file.id = symbol.file_id "
        "JOIN index_runs run ON run.id = file.run_id "
        "JOIN repositories repository ON repository.active_run_id = run.id "
        "WHERE repository.canonical_path = ?1 AND symbol.role = 'definition' AND symbol." +
        std::string(name_column) +
        " = ?2 "
        "ORDER BY symbol.qualified_name, file.relative_path, symbol.start_byte";
    Statement statement(database_, sql.c_str());
    bind_text(database_, statement.get(), 1, canonical_repository_path);
    bind_text(database_, statement.get(), 2, query);

    std::vector<StoredQuerySymbol> symbols;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
      symbols.push_back(StoredQuerySymbol{
          .id = sqlite3_column_int64(statement.get(), 0),
          .symbol = column_symbol(statement.get(), 1),
      });
    }
    return symbols;
  };

  auto symbols = find("qualified_name");
  if (symbols.empty()) {
    symbols = find("name");
  }
  return symbols;
}

std::vector<CallQueryEntry> SqliteDatabase::query_calls(std::int64_t symbol_id,
                                                        CallQueryDirection direction) const {
  const char* filter = direction == CallQueryDirection::kCallees
                           ? "relationship.source_symbol_id = ?1"
                           : "(relationship.target_symbol_id = ?1 OR EXISTS ("
                             "SELECT 1 FROM relationship_candidates matching_candidate "
                             "WHERE matching_candidate.relationship_id = relationship.id "
                             "AND matching_candidate.target_symbol_id = ?1))";
  const std::string sql =
      "SELECT relationship.id, relationship.target_text, relationship.resolution, "
      "relationship.confidence, relationship.provenance, relationship.start_line, "
      "relationship.start_column, relationship.end_line, relationship.end_column, "
      "relationship.start_byte, relationship.end_byte, "
      "source_symbol.kind, source_symbol.name, source_symbol.qualified_name, "
      "source_symbol.signature, source_file.relative_path, source_symbol.start_line, "
      "source_symbol.start_column, source_symbol.end_line, source_symbol.end_column, "
      "source_symbol.start_byte, source_symbol.end_byte, "
      "target_symbol.kind, target_symbol.name, target_symbol.qualified_name, "
      "target_symbol.signature, target_file.relative_path, target_symbol.start_line, "
      "target_symbol.start_column, target_symbol.end_line, target_symbol.end_column, "
      "target_symbol.start_byte, target_symbol.end_byte "
      "FROM relationships relationship "
      "JOIN symbols source_symbol ON source_symbol.id = relationship.source_symbol_id "
      "JOIN files source_file ON source_file.id = source_symbol.file_id "
      "LEFT JOIN symbols target_symbol ON target_symbol.id = relationship.target_symbol_id "
      "LEFT JOIN files target_file ON target_file.id = target_symbol.file_id "
      "WHERE relationship.kind = 'calls' AND " +
      std::string(filter) +
      " ORDER BY source_file.relative_path, relationship.start_byte, relationship.id";
  Statement statement(database_, sql.c_str());
  check(database_, sqlite3_bind_int64(statement.get(), 1, symbol_id));

  Statement candidate_statement(
      database_,
      "SELECT symbol.kind, symbol.name, symbol.qualified_name, symbol.signature, "
      "file.relative_path, symbol.start_line, symbol.start_column, symbol.end_line, "
      "symbol.end_column, symbol.start_byte, symbol.end_byte, candidate.reason, candidate.rank "
      "FROM relationship_candidates candidate "
      "JOIN symbols symbol ON symbol.id = candidate.target_symbol_id "
      "JOIN files file ON file.id = symbol.file_id "
      "WHERE candidate.relationship_id = ?1 ORDER BY candidate.rank");

  std::vector<CallQueryEntry> calls;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    CallQueryEntry call{
        .caller = column_symbol(statement.get(), 11),
        .callee = std::nullopt,
        .target_text = column_text(statement.get(), 1),
        .resolution = resolution_status(column_text(statement.get(), 2)),
        .confidence = confidence(column_text(statement.get(), 3)),
        .provenance = column_text(statement.get(), 4),
        .evidence = column_range(statement.get(), 5),
        .candidates = {},
    };
    if (sqlite3_column_type(statement.get(), 22) != SQLITE_NULL) {
      call.callee = column_symbol(statement.get(), 22);
    }

    sqlite3_reset(candidate_statement.get());
    sqlite3_clear_bindings(candidate_statement.get());
    check(database_, sqlite3_bind_int64(candidate_statement.get(), 1,
                                        sqlite3_column_int64(statement.get(), 0)));
    while (sqlite3_step(candidate_statement.get()) == SQLITE_ROW) {
      call.candidates.push_back(CallQueryCandidate{
          .symbol = column_symbol(candidate_statement.get(), 0),
          .reason = column_text(candidate_statement.get(), 11),
          .rank = static_cast<std::uint32_t>(sqlite3_column_int64(candidate_statement.get(), 12)),
      });
    }
    calls.push_back(std::move(call));
  }
  return calls;
}

GraphSnapshot SqliteDatabase::load_graph(const std::string& canonical_repository_path) const {
  GraphSnapshot graph;

  {
    Statement statement(database_,
                        "SELECT file.id, file.relative_path "
                        "FROM files file "
                        "JOIN repositories repository ON repository.active_run_id = file.run_id "
                        "WHERE repository.canonical_path = ?1 ORDER BY file.relative_path");
    bind_text(database_, statement.get(), 1, canonical_repository_path);
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
      graph.files.push_back(GraphFileNode{
          .id = sqlite3_column_int64(statement.get(), 0),
          .path = column_text(statement.get(), 1),
      });
    }
  }

  {
    Statement statement(
        database_,
        "SELECT symbol.id, file.id, symbol.kind, symbol.name, symbol.qualified_name, "
        "symbol.signature, file.relative_path, symbol.start_line, symbol.start_column, "
        "symbol.end_line, symbol.end_column, symbol.start_byte, symbol.end_byte "
        "FROM symbols symbol "
        "JOIN files file ON file.id = symbol.file_id "
        "JOIN repositories repository ON repository.active_run_id = file.run_id "
        "WHERE repository.canonical_path = ?1 AND symbol.role = 'definition' "
        "ORDER BY symbol.qualified_name, file.relative_path, symbol.start_byte");
    bind_text(database_, statement.get(), 1, canonical_repository_path);
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
      graph.symbols.push_back(GraphSymbolNode{
          .id = sqlite3_column_int64(statement.get(), 0),
          .file_id = sqlite3_column_int64(statement.get(), 1),
          .symbol = column_symbol(statement.get(), 2),
      });
    }
  }

  std::map<std::int64_t, std::vector<GraphCandidate>> call_candidates;
  std::map<std::int64_t, std::vector<GraphCandidate>> include_candidates;
  {
    Statement statement(
        database_,
        "SELECT candidate.relationship_id, relationship.kind, candidate.target_symbol_id, "
        "candidate.target_file_id, candidate.reason, candidate.rank "
        "FROM relationship_candidates candidate "
        "JOIN relationships relationship ON relationship.id = candidate.relationship_id "
        "JOIN repositories repository ON repository.active_run_id = relationship.run_id "
        "WHERE repository.canonical_path = ?1 ORDER BY candidate.relationship_id, candidate.rank");
    bind_text(database_, statement.get(), 1, canonical_repository_path);
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
      const auto relationship_id = sqlite3_column_int64(statement.get(), 0);
      const auto kind = column_text(statement.get(), 1);
      if (kind == "calls" && sqlite3_column_type(statement.get(), 2) != SQLITE_NULL) {
        call_candidates[relationship_id].push_back(GraphCandidate{
            .node_id = sqlite3_column_int64(statement.get(), 2),
            .reason = column_text(statement.get(), 4),
            .rank = static_cast<std::uint32_t>(sqlite3_column_int64(statement.get(), 5)),
        });
      } else if (kind == "includes" && sqlite3_column_type(statement.get(), 3) != SQLITE_NULL) {
        include_candidates[relationship_id].push_back(GraphCandidate{
            .node_id = sqlite3_column_int64(statement.get(), 3),
            .reason = column_text(statement.get(), 4),
            .rank = static_cast<std::uint32_t>(sqlite3_column_int64(statement.get(), 5)),
        });
      }
    }
  }

  {
    Statement statement(
        database_,
        "SELECT relationship.id, relationship.source_symbol_id, relationship.target_symbol_id, "
        "relationship.target_text, relationship.resolution, relationship.confidence, "
        "relationship.provenance, relationship.start_line, relationship.start_column, "
        "relationship.end_line, relationship.end_column, relationship.start_byte, "
        "relationship.end_byte "
        "FROM relationships relationship "
        "JOIN repositories repository ON repository.active_run_id = relationship.run_id "
        "WHERE repository.canonical_path = ?1 AND relationship.kind = 'calls' "
        "ORDER BY relationship.source_symbol_id, relationship.start_byte, relationship.id");
    bind_text(database_, statement.get(), 1, canonical_repository_path);
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
      const auto relationship_id = sqlite3_column_int64(statement.get(), 0);
      graph.calls.push_back(GraphCallEdge{
          .source_symbol_id = sqlite3_column_int64(statement.get(), 1),
          .target_symbol_id =
              sqlite3_column_type(statement.get(), 2) == SQLITE_NULL
                  ? std::nullopt
                  : std::optional<GraphNodeId>{sqlite3_column_int64(statement.get(), 2)},
          .target_text = column_text(statement.get(), 3),
          .resolution = resolution_status(column_text(statement.get(), 4)),
          .confidence = confidence(column_text(statement.get(), 5)),
          .provenance = column_text(statement.get(), 6),
          .evidence = column_range(statement.get(), 7),
          .candidates = call_candidates[relationship_id],
      });
    }
  }

  {
    Statement statement(
        database_,
        "SELECT relationship.id, relationship.source_file_id, relationship.target_file_id, "
        "relationship.target_text, relationship.resolution, relationship.confidence, "
        "relationship.provenance, relationship.start_line, relationship.start_column, "
        "relationship.end_line, relationship.end_column, relationship.start_byte, "
        "relationship.end_byte "
        "FROM relationships relationship "
        "JOIN repositories repository ON repository.active_run_id = relationship.run_id "
        "WHERE repository.canonical_path = ?1 AND relationship.kind = 'includes' "
        "ORDER BY relationship.source_file_id, relationship.start_byte, relationship.id");
    bind_text(database_, statement.get(), 1, canonical_repository_path);
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
      const auto relationship_id = sqlite3_column_int64(statement.get(), 0);
      graph.includes.push_back(GraphIncludeEdge{
          .source_file_id = sqlite3_column_int64(statement.get(), 1),
          .target_file_id =
              sqlite3_column_type(statement.get(), 2) == SQLITE_NULL
                  ? std::nullopt
                  : std::optional<GraphNodeId>{sqlite3_column_int64(statement.get(), 2)},
          .target_text = column_text(statement.get(), 3),
          .resolution = resolution_status(column_text(statement.get(), 4)),
          .confidence = confidence(column_text(statement.get(), 5)),
          .provenance = column_text(statement.get(), 6),
          .evidence = column_range(statement.get(), 7),
          .candidates = include_candidates[relationship_id],
      });
    }
  }

  return graph;
}

void SqliteDatabase::replace_index(const std::filesystem::path& repository_path,
                                   const std::vector<IndexedFile>& files,
                                   const std::vector<RelationshipRecord>& relationships) {
  execute("BEGIN IMMEDIATE");
  try {
    const auto canonical_path = std::filesystem::canonical(repository_path).generic_string();

    {
      Statement statement(database_,
                          "INSERT INTO repositories(canonical_path) VALUES (?1) "
                          "ON CONFLICT(canonical_path) DO NOTHING");
      bind_text(database_, statement.get(), 1, canonical_path);
      check(database_, sqlite3_step(statement.get()));
    }

    const auto repo_id = repository_id(database_, canonical_path);
    std::int64_t run_id = 0;
    {
      Statement statement(database_,
                          "INSERT INTO index_runs(repository_id, started_at, status) "
                          "VALUES (?1, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), 'running')");
      check(database_, sqlite3_bind_int64(statement.get(), 1, repo_id));
      check(database_, sqlite3_step(statement.get()));
      run_id = sqlite3_last_insert_rowid(database_);
    }

    Statement insert_file(
        database_,
        "INSERT INTO files(run_id, relative_path, language, content_fingerprint, size_bytes, "
        "parse_status) VALUES (?1, ?2, ?3, ?4, ?5, ?6)");
    Statement insert_symbol(
        database_,
        "INSERT INTO symbols(file_id, kind, role, name, qualified_name, signature, start_line, "
        "start_column, end_line, end_column, start_byte, end_byte) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)");
    Statement insert_include(
        database_,
        "INSERT INTO include_directives(file_id, target, is_system, start_line, start_column, "
        "end_line, end_column, start_byte, end_byte) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)");
    Statement insert_diagnostic(
        database_,
        "INSERT INTO diagnostics(file_id, severity, code, message, start_line, start_column, "
        "end_line, end_column, start_byte, end_byte) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)");

    std::map<std::string, std::int64_t> file_ids;
    std::map<SymbolDatabaseKey, std::int64_t> symbol_ids;
    for (const auto& indexed_file : files) {
      const auto& file = indexed_file.file;
      const auto& analysis = indexed_file.analysis;
      sqlite3_reset(insert_file.get());
      sqlite3_clear_bindings(insert_file.get());
      check(database_, sqlite3_bind_int64(insert_file.get(), 1, run_id));
      bind_text(database_, insert_file.get(), 2, file.relative_path);
      bind_text(database_, insert_file.get(), 3, file.language);
      bind_text(database_, insert_file.get(), 4, file.content_fingerprint);
      check(database_,
            sqlite3_bind_int64(insert_file.get(), 5, static_cast<sqlite3_int64>(file.size_bytes)));
      const std::string parse_status =
          analysis.status == AnalysisStatus::kFailed
              ? "failed"
              : (analysis.diagnostics.empty() ? "parsed" : "parsed_with_errors");
      bind_text(database_, insert_file.get(), 6, parse_status);
      check(database_, sqlite3_step(insert_file.get()));
      const auto file_id = sqlite3_last_insert_rowid(database_);
      file_ids.emplace(file.relative_path, file_id);

      for (const auto& symbol : analysis.symbols) {
        sqlite3_reset(insert_symbol.get());
        sqlite3_clear_bindings(insert_symbol.get());
        check(database_, sqlite3_bind_int64(insert_symbol.get(), 1, file_id));
        bind_text(database_, insert_symbol.get(), 2, to_string(symbol.kind));
        bind_text(database_, insert_symbol.get(), 3, to_string(symbol.role));
        bind_text(database_, insert_symbol.get(), 4, symbol.name);
        bind_text(database_, insert_symbol.get(), 5, symbol.qualified_name);
        bind_text(database_, insert_symbol.get(), 6, symbol.signature);
        bind_range(database_, insert_symbol.get(), 7, symbol.range);
        check(database_, sqlite3_step(insert_symbol.get()));
        symbol_ids.emplace(SymbolDatabaseKey(file.relative_path, symbol.qualified_name, symbol.role,
                                             symbol.range.start_byte),
                           sqlite3_last_insert_rowid(database_));
      }

      for (const auto& include : analysis.includes) {
        sqlite3_reset(insert_include.get());
        sqlite3_clear_bindings(insert_include.get());
        check(database_, sqlite3_bind_int64(insert_include.get(), 1, file_id));
        bind_text(database_, insert_include.get(), 2, include.target);
        check(database_, sqlite3_bind_int(insert_include.get(), 3, include.is_system ? 1 : 0));
        bind_range(database_, insert_include.get(), 4, include.range);
        check(database_, sqlite3_step(insert_include.get()));
      }

      for (const auto& diagnostic : analysis.diagnostics) {
        sqlite3_reset(insert_diagnostic.get());
        sqlite3_clear_bindings(insert_diagnostic.get());
        check(database_, sqlite3_bind_int64(insert_diagnostic.get(), 1, file_id));
        bind_text(database_, insert_diagnostic.get(), 2, to_string(diagnostic.severity));
        bind_text(database_, insert_diagnostic.get(), 3, diagnostic.code);
        bind_text(database_, insert_diagnostic.get(), 4, diagnostic.message);
        bind_range(database_, insert_diagnostic.get(), 5, diagnostic.range);
        check(database_, sqlite3_step(insert_diagnostic.get()));
      }
    }

    Statement insert_relationship(
        database_,
        "INSERT INTO relationships(run_id, kind, source_file_id, source_symbol_id, "
        "target_file_id, target_symbol_id, target_text, resolution, confidence, provenance, "
        "candidate_count, start_line, start_column, end_line, end_column, start_byte, end_byte) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17)");
    Statement insert_candidate(
        database_,
        "INSERT INTO relationship_candidates(relationship_id, target_file_id, target_symbol_id, "
        "reason, rank) VALUES (?1, ?2, ?3, ?4, ?5)");

    for (const auto& relationship : relationships) {
      sqlite3_reset(insert_relationship.get());
      sqlite3_clear_bindings(insert_relationship.get());
      check(database_, sqlite3_bind_int64(insert_relationship.get(), 1, run_id));
      bind_text(database_, insert_relationship.get(), 2, to_string(relationship.kind));
      check(database_,
            sqlite3_bind_int64(insert_relationship.get(), 3,
                               require_file_id(file_ids, relationship.source_file_path)));
      bind_optional_id(database_, insert_relationship.get(), 4,
                       relationship.source_symbol ? std::optional<std::int64_t>{require_symbol_id(
                                                        symbol_ids, *relationship.source_symbol)}
                                                  : std::nullopt);
      bind_optional_id(database_, insert_relationship.get(), 5,
                       relationship.target_file_path
                           ? std::optional<std::int64_t>{require_file_id(
                                 file_ids, *relationship.target_file_path)}
                           : std::nullopt);
      bind_optional_id(database_, insert_relationship.get(), 6,
                       relationship.target_symbol ? std::optional<std::int64_t>{require_symbol_id(
                                                        symbol_ids, *relationship.target_symbol)}
                                                  : std::nullopt);
      bind_text(database_, insert_relationship.get(), 7, relationship.target_text);
      bind_text(database_, insert_relationship.get(), 8, to_string(relationship.resolution));
      bind_text(database_, insert_relationship.get(), 9, to_string(relationship.confidence));
      bind_text(database_, insert_relationship.get(), 10, relationship.provenance);
      check(database_,
            sqlite3_bind_int64(insert_relationship.get(), 11,
                               static_cast<sqlite3_int64>(relationship.candidates.size())));
      bind_range(database_, insert_relationship.get(), 12, relationship.evidence);
      check(database_, sqlite3_step(insert_relationship.get()));
      const auto relationship_id = sqlite3_last_insert_rowid(database_);

      for (const auto& candidate : relationship.candidates) {
        sqlite3_reset(insert_candidate.get());
        sqlite3_clear_bindings(insert_candidate.get());
        check(database_, sqlite3_bind_int64(insert_candidate.get(), 1, relationship_id));
        bind_optional_id(database_, insert_candidate.get(), 2,
                         candidate.target_file_path ? std::optional<std::int64_t>{require_file_id(
                                                          file_ids, *candidate.target_file_path)}
                                                    : std::nullopt);
        bind_optional_id(database_, insert_candidate.get(), 3,
                         candidate.target_symbol ? std::optional<std::int64_t>{require_symbol_id(
                                                       symbol_ids, *candidate.target_symbol)}
                                                 : std::nullopt);
        bind_text(database_, insert_candidate.get(), 4, candidate.reason);
        check(database_, sqlite3_bind_int64(insert_candidate.get(), 5, candidate.rank));
        check(database_, sqlite3_step(insert_candidate.get()));
      }
    }

    {
      Statement statement(
          database_,
          "UPDATE index_runs SET completed_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), "
          "status = 'completed' WHERE id = ?1");
      check(database_, sqlite3_bind_int64(statement.get(), 1, run_id));
      check(database_, sqlite3_step(statement.get()));
    }
    {
      Statement statement(database_, "UPDATE repositories SET active_run_id = ?1 WHERE id = ?2");
      check(database_, sqlite3_bind_int64(statement.get(), 1, run_id));
      check(database_, sqlite3_bind_int64(statement.get(), 2, repo_id));
      check(database_, sqlite3_step(statement.get()));
    }
    {
      Statement statement(database_,
                          "DELETE FROM index_runs WHERE repository_id = ?1 AND id <> ?2");
      check(database_, sqlite3_bind_int64(statement.get(), 1, repo_id));
      check(database_, sqlite3_bind_int64(statement.get(), 2, run_id));
      check(database_, sqlite3_step(statement.get()));
    }

    execute("COMMIT");
  } catch (...) {
    try {
      execute("ROLLBACK");
    } catch (...) {
    }
    throw;
  }
}

}  // namespace sherpa
