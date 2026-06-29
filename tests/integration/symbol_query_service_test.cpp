#include "sherpa/application/symbol_query_service.hpp"

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
    path_ = std::filesystem::temp_directory_path() /
            ("sherpa-symbol-query-test-" + std::to_string(suffix));
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

}  // namespace

TEST_CASE("symbol query returns symbol metadata and call counts") {
  QueryTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = relationship_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = sherpa::SymbolQueryService{}.query({
      .symbol = "run",
      .repository_path = fixture,
      .database_path = database_path,
  });

  REQUIRE(result.symbol.qualified_name == "run");
  REQUIRE(result.symbol.file_path == "src/main.cpp");
  REQUIRE(result.callers.resolved == 0);
  REQUIRE(result.callers.ambiguous == 0);
  REQUIRE(result.callees.resolved == 1);
  REQUIRE(result.callees.ambiguous == 1);
  REQUIRE(result.callees.unresolved == 1);
}

TEST_CASE("symbol query reports missing and ambiguous symbols") {
  QueryTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = relationship_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  REQUIRE_THROWS_AS(sherpa::SymbolQueryService{}.query({
                        .symbol = "unknown",
                        .repository_path = fixture,
                        .database_path = database_path,
                    }),
                    sherpa::SymbolNotFoundError);

  REQUIRE_THROWS_AS(sherpa::SymbolQueryService{}.query({
                        .symbol = "overloaded",
                        .repository_path = fixture,
                        .database_path = database_path,
                    }),
                    sherpa::AmbiguousSymbolError);
}
