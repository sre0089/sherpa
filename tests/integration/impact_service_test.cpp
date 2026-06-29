#include "sherpa/application/impact_service.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <string>

#include "sherpa/application/index_service.hpp"

#ifndef SHERPA_SOURCE_DIR
#define SHERPA_SOURCE_DIR "."
#endif

namespace {

class ImpactTemporaryDirectory {
 public:
  ImpactTemporaryDirectory() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ =
        std::filesystem::temp_directory_path() / ("sherpa-impact-test-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~ImpactTemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

std::filesystem::path impact_fixture() {
  return std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "impact";
}

sherpa::ImpactResult analyze(const std::string& target, const std::filesystem::path& fixture,
                             const std::filesystem::path& database_path) {
  return sherpa::ImpactService{}.analyze({
      .target = target,
      .repository_path = fixture,
      .database_path = database_path,
  });
}

}  // namespace

TEST_CASE("impact service traverses transitive callers in breadth-first order") {
  ImpactTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = impact_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = analyze("leaf", fixture, database_path);

  REQUIRE(result.target.kind == sherpa::ImpactNodeKind::kSymbol);
  REQUIRE(result.confirmed.size() == 3);
  REQUIRE(result.confirmed[0].node.symbol->qualified_name == "middle");
  REQUIRE(result.confirmed[0].depth == 1);
  REQUIRE(result.confirmed[1].node.symbol->qualified_name == "top");
  REQUIRE(result.confirmed[1].depth == 2);
  REQUIRE(result.confirmed[2].node.symbol->qualified_name == "run");
  REQUIRE(result.confirmed[2].depth == 3);
  REQUIRE(result.possible.empty());
}

TEST_CASE("impact service traverses transitive includers for absolute file input") {
  ImpactTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = impact_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = analyze((fixture / "include" / "base.hpp").string(), fixture, database_path);

  REQUIRE(result.target.kind == sherpa::ImpactNodeKind::kFile);
  REQUIRE(result.target.file_path == "include/base.hpp");
  REQUIRE(result.confirmed.size() == 3);
  REQUIRE(result.confirmed[0].node.file_path == "include/wrapper.hpp");
  REQUIRE(result.confirmed[0].depth == 1);
  REQUIRE(result.confirmed[1].node.file_path == "src/graph.cpp");
  REQUIRE(result.confirmed[1].depth == 2);
  REQUIRE(result.confirmed[2].node.file_path == "src/main.cpp");
  REQUIRE(result.confirmed[2].depth == 2);

  const auto source_result = analyze("src/graph.cpp", fixture, database_path);
  REQUIRE(source_result.confirmed.size() == 1);
  REQUIRE(source_result.confirmed.front().node.symbol->qualified_name == "run");
  REQUIRE(source_result.confirmed.front().relationship == sherpa::RelationshipKind::kCalls);
}

TEST_CASE("impact service is cycle-safe") {
  ImpactTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = impact_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = analyze("cycle_a", fixture, database_path);

  REQUIRE(result.confirmed.size() == 1);
  REQUIRE(result.confirmed.front().node.symbol->qualified_name == "cycle_b");
  REQUIRE(result.confirmed.front().depth == 1);
}

TEST_CASE("impact service reports ambiguous calls and includes without traversing them") {
  ImpactTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = impact_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto symbol_result = analyze("alpha::candidate", fixture, database_path);
  REQUIRE(symbol_result.confirmed.empty());
  REQUIRE(symbol_result.possible.size() == 1);
  REQUIRE(symbol_result.possible.front().node.symbol->qualified_name == "maybe");
  REQUIRE(symbol_result.possible.front().relationship == sherpa::RelationshipKind::kCalls);

  const auto file_result = analyze("include/a/common.hpp", fixture, database_path);
  REQUIRE(file_result.confirmed.empty());
  REQUIRE(file_result.possible.size() == 1);
  REQUIRE(file_result.possible.front().node.file_path == "src/ambiguous.cpp");
  REQUIRE(file_result.possible.front().relationship == sherpa::RelationshipKind::kIncludes);
}

TEST_CASE("impact service reports missing and ambiguous targets") {
  ImpactTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture = impact_fixture();
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  REQUIRE_THROWS_AS(analyze("unknown", fixture, database_path), sherpa::ImpactTargetNotFoundError);
  REQUIRE_THROWS_AS(analyze("candidate", fixture, database_path), sherpa::AmbiguousSymbolError);
}

TEST_CASE("impact service disambiguates an overloaded symbol by signature") {
  ImpactTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto fixture =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "relationships";
  static_cast<void>(sherpa::IndexService{}.index({fixture, database_path}));

  const auto result = sherpa::ImpactService{}.analyze({
      .target = "overloaded",
      .signature = "overloaded(double value)",
      .repository_path = fixture,
      .database_path = database_path,
  });

  REQUIRE(result.target.symbol->signature == "overloaded(double value)");
}
