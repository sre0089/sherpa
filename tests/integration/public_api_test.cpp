#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string>

#include "sherpa/api/client.hpp"

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ =
        std::filesystem::temp_directory_path() / ("sherpa-public-api-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

}  // namespace

TEST_CASE("public API indexes and queries without exposing infrastructure") {
  TemporaryDirectory temporary;
  const auto repository =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "relationships";
  const auto database = temporary.path() / "api.sqlite";
  const sherpa::api::Client client;

  const auto indexed = client.index({
      .repository_path = repository,
      .database_path = database,
  });
  REQUIRE(indexed.indexed_files == 5);
  REQUIRE(indexed.parsed_files == 5);

  const sherpa::api::Repository indexed_repository{
      .path = repository,
      .database_path = database,
  };
  const auto symbol = client.symbol({
      .repository = indexed_repository,
      .symbol = {.name = "run"},
  });
  REQUIRE(symbol.symbol.qualified_name == "run");

  const auto calls = client.callees({
      .repository = indexed_repository,
      .symbol = {.name = "run"},
  });
  REQUIRE_FALSE(calls.calls.empty());

  const auto graph = client.graph(indexed_repository);
  REQUIRE_FALSE(graph.nodes.empty());
  REQUIRE_FALSE(graph.edges.empty());
}

TEST_CASE("public API reports stable error categories and ambiguity candidates") {
  TemporaryDirectory temporary;
  const auto repository =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "relationships";
  const auto database = temporary.path() / "api-errors.sqlite";
  const sherpa::api::Client client;
  static_cast<void>(client.index({
      .repository_path = repository,
      .database_path = database,
  }));
  const sherpa::api::Repository indexed_repository{
      .path = repository,
      .database_path = database,
  };

  try {
    static_cast<void>(client.symbol({
        .repository = indexed_repository,
        .symbol = {.name = "overloaded"},
    }));
    FAIL("expected an ambiguous symbol error");
  } catch (const sherpa::api::Error& error) {
    REQUIRE(error.code() == sherpa::api::ErrorCode::kAmbiguousSymbol);
    REQUIRE(error.candidates().size() == 2);
  }

  try {
    static_cast<void>(client.symbol({
        .repository = indexed_repository,
        .symbol = {.name = "missing"},
    }));
    FAIL("expected a not-found error");
  } catch (const sherpa::api::Error& error) {
    REQUIRE(error.code() == sherpa::api::ErrorCode::kNotFound);
    REQUIRE(error.candidates().empty());
  }
}
