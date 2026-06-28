#include "sherpa/application/call_query_service.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <string>

#include "sherpa/application/index_service.hpp"

#ifndef SHERPA_SOURCE_DIR
#define SHERPA_SOURCE_DIR "."
#endif

namespace {

class QueryTemporaryDirectory {
 public:
  QueryTemporaryDirectory() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ =
        std::filesystem::temp_directory_path() / ("sherpa-query-test-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~QueryTemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

std::filesystem::path relationship_fixture() {
  return std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "relationships";
}

std::filesystem::path query_ambiguity_fixture() {
  return std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "query_ambiguity";
}

}  // namespace

TEST_CASE("call query service returns resolved ambiguous and unresolved callees") {
  QueryTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = relationship_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = sherpa::CallQueryService{}.query({
      .direction = sherpa::CallQueryDirection::kCallees,
      .symbol = "run",
      .repository_path = fixture,
      .database_path = database_path,
  });

  REQUIRE(result.symbol.qualified_name == "run");
  REQUIRE(result.calls.size() == 3);
  REQUIRE(result.calls[0].target_text == "target");
  REQUIRE(result.calls[0].resolution == sherpa::ResolutionStatus::kResolved);
  REQUIRE(result.calls[0].callee->file_path == "src/api.cpp");
  REQUIRE(result.calls[1].target_text == "overloaded");
  REQUIRE(result.calls[1].resolution == sherpa::ResolutionStatus::kAmbiguous);
  REQUIRE(result.calls[1].candidates.size() == 2);
  REQUIRE(result.calls[2].target_text == "missing");
  REQUIRE(result.calls[2].resolution == sherpa::ResolutionStatus::kUnresolved);
}

TEST_CASE("call query service returns incoming callers") {
  QueryTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = relationship_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = sherpa::CallQueryService{}.query({
      .direction = sherpa::CallQueryDirection::kCallers,
      .symbol = "target",
      .repository_path = fixture,
      .database_path = database_path,
  });

  REQUIRE(result.calls.size() == 1);
  REQUIRE(result.calls.front().caller.qualified_name == "run");
  REQUIRE(result.calls.front().evidence.start_line == 6);
}

TEST_CASE("callers includes relationships where a qualified symbol is an ambiguity candidate") {
  QueryTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = query_ambiguity_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = sherpa::CallQueryService{}.query({
      .direction = sherpa::CallQueryDirection::kCallers,
      .symbol = "alpha::target",
      .repository_path = fixture,
      .database_path = database_path,
  });

  REQUIRE(result.symbol.qualified_name == "alpha::target");
  REQUIRE(result.calls.size() == 1);
  REQUIRE(result.calls.front().caller.qualified_name == "run");
  REQUIRE(result.calls.front().resolution == sherpa::ResolutionStatus::kAmbiguous);
  REQUIRE(result.calls.front().candidates.size() == 2);
}

TEST_CASE("call query service reports missing and ambiguous symbols") {
  QueryTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = relationship_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  REQUIRE_THROWS_AS(sherpa::CallQueryService{}.query({
                        .direction = sherpa::CallQueryDirection::kCallees,
                        .symbol = "unknown",
                        .repository_path = fixture,
                        .database_path = database_path,
                    }),
                    sherpa::SymbolNotFoundError);

  try {
    static_cast<void>(sherpa::CallQueryService{}.query({
        .direction = sherpa::CallQueryDirection::kCallees,
        .symbol = "overloaded",
        .repository_path = fixture,
        .database_path = database_path,
    }));
    FAIL("expected ambiguous symbol");
  } catch (const sherpa::AmbiguousSymbolError& error) {
    REQUIRE(error.candidates().size() == 2);
    REQUIRE(error.candidates()[0].signature == "overloaded(int value)");
    REQUIRE(error.candidates()[1].signature == "overloaded(double value)");
  }
}

TEST_CASE("call query service does not create a missing index") {
  QueryTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "missing.sqlite";

  REQUIRE_THROWS_AS(sherpa::CallQueryService{}.query({
                        .direction = sherpa::CallQueryDirection::kCallers,
                        .symbol = "target",
                        .repository_path = relationship_fixture(),
                        .database_path = database_path,
                    }),
                    sherpa::IndexUnavailableError);
  REQUIRE_FALSE(std::filesystem::exists(database_path));
}
