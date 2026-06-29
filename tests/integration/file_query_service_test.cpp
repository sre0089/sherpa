#include "sherpa/application/file_query_service.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <ranges>
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
        std::filesystem::temp_directory_path() / ("sherpa-file-query-test-" + std::to_string(suffix));
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

TEST_CASE("file query returns definitions and direct include edges") {
  QueryTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = relationship_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = sherpa::FileQueryService{}.query({
      .path = "src/main.cpp",
      .repository_path = fixture,
      .database_path = database_path,
  });

  REQUIRE(result.path == "src/main.cpp");
  REQUIRE(result.definitions.size() == 1);
  REQUIRE(result.definitions.front().qualified_name == "run");
  REQUIRE(result.outgoing_includes.size() == 3);
  const auto resolved =
      std::ranges::find(result.outgoing_includes, std::string("include/api.hpp"),
                        [](const sherpa::FileIncludeRecord& record) {
                          return record.target_path.value_or("");
                        });
  REQUIRE(resolved != result.outgoing_includes.end());

  const auto ambiguous =
      std::ranges::find(result.outgoing_includes, sherpa::ResolutionStatus::kAmbiguous,
                        &sherpa::FileIncludeRecord::resolution);
  REQUIRE(ambiguous != result.outgoing_includes.end());
  REQUIRE(ambiguous->candidates.size() == 2);

  const auto unresolved =
      std::ranges::find(result.outgoing_includes, sherpa::ResolutionStatus::kUnresolved,
                        &sherpa::FileIncludeRecord::resolution);
  REQUIRE(unresolved != result.outgoing_includes.end());
}

TEST_CASE("file query accepts repository absolute paths and reports missing files") {
  QueryTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = relationship_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = sherpa::FileQueryService{}.query({
      .path = (fixture / "src" / "main.cpp").string(),
      .repository_path = fixture,
      .database_path = database_path,
  });
  REQUIRE(result.path == "src/main.cpp");

  REQUIRE_THROWS_AS(sherpa::FileQueryService{}.query({
                        .path = "src/missing.cpp",
                        .repository_path = fixture,
                        .database_path = database_path,
                    }),
                    sherpa::FileNotFoundError);
}
