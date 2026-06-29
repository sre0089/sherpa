#include "sherpa/application/path_service.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <string>

#include "sherpa/application/index_service.hpp"

#ifndef SHERPA_SOURCE_DIR
#define SHERPA_SOURCE_DIR "."
#endif

namespace {

class PathTemporaryDirectory {
 public:
  PathTemporaryDirectory() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() / ("sherpa-path-test-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~PathTemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

std::filesystem::path path_fixture() {
  return std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "impact";
}

sherpa::PathResult find_path(const std::string& source, const std::string& target,
                             const std::filesystem::path& fixture,
                             const std::filesystem::path& database_path) {
  return sherpa::PathService{}.find({
      .source = source,
      .target = target,
      .repository_path = fixture,
      .database_path = database_path,
  });
}

}  // namespace

TEST_CASE("path service finds the shortest confirmed directed call path") {
  PathTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = path_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = find_path("run", "leaf", fixture, database_path);

  REQUIRE(result.status == sherpa::PathStatus::kConfirmed);
  REQUIRE(result.steps.size() == 3);
  REQUIRE(result.steps[0].source.qualified_name == "run");
  REQUIRE(result.steps[0].target.qualified_name == "top");
  REQUIRE(result.steps[1].target.qualified_name == "middle");
  REQUIRE(result.steps[2].target.qualified_name == "leaf");
  REQUIRE(result.steps[2].resolution == sherpa::ResolutionStatus::kResolved);
}

TEST_CASE("path service reports a possible path through an ambiguity candidate") {
  PathTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = path_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = find_path("maybe", "alpha::candidate", fixture, database_path);

  REQUIRE(result.status == sherpa::PathStatus::kPossible);
  REQUIRE(result.steps.size() == 1);
  REQUIRE(result.steps.front().resolution == sherpa::ResolutionStatus::kAmbiguous);
  REQUIRE(result.steps.front().target.qualified_name == "alpha::candidate");
}

TEST_CASE("path service prefers a longer confirmed path over a shorter possible path") {
  PathTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = path_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = find_path("certainty", "alpha::endpoint", fixture, database_path);

  REQUIRE(result.status == sherpa::PathStatus::kConfirmed);
  REQUIRE(result.steps.size() == 2);
  REQUIRE(result.steps[0].target.qualified_name == "bridge");
  REQUIRE(result.steps[1].target.qualified_name == "alpha::endpoint");
}

TEST_CASE("path service handles cycles identity paths and disconnected symbols") {
  PathTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = path_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto cyclic = find_path("cycle_a", "cycle_b", fixture, database_path);
  REQUIRE(cyclic.status == sherpa::PathStatus::kConfirmed);
  REQUIRE(cyclic.steps.size() == 1);

  const auto identity = find_path("leaf", "leaf", fixture, database_path);
  REQUIRE(identity.status == sherpa::PathStatus::kConfirmed);
  REQUIRE(identity.steps.empty());

  const auto disconnected = find_path("leaf", "run", fixture, database_path);
  REQUIRE(disconnected.status == sherpa::PathStatus::kNotFound);
  REQUIRE(disconnected.steps.empty());
}

TEST_CASE("path service reports missing and ambiguous endpoints") {
  PathTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = path_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  REQUIRE_THROWS_AS(find_path("unknown", "leaf", fixture, database_path),
                    sherpa::SymbolNotFoundError);
  REQUIRE_THROWS_AS(find_path("run", "candidate", fixture, database_path),
                    sherpa::AmbiguousSymbolError);
}
