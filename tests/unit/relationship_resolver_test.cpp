#include "sherpa/analysis/relationship_resolver.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>
#include <vector>

#include "sherpa/parsing/tree_sitter_frontend.hpp"
#include "sherpa/scanner/repository_scanner.hpp"

#ifndef SHERPA_SOURCE_DIR
#define SHERPA_SOURCE_DIR "."
#endif

namespace {

std::vector<sherpa::IndexedFile> relationship_fixture() {
  const auto root =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "relationships";
  const auto scan = sherpa::RepositoryScanner{}.scan(root);
  sherpa::TreeSitterFrontend frontend;
  std::vector<sherpa::IndexedFile> files;
  for (const auto& file : scan.files) {
    files.push_back(sherpa::IndexedFile{
        .file = file,
        .analysis = frontend.analyze(file),
    });
  }
  return files;
}

const sherpa::RelationshipRecord* find_relationship(
    const std::vector<sherpa::RelationshipRecord>& relationships, sherpa::RelationshipKind kind,
    const std::string& source_file, const std::string& target_text) {
  for (const auto& relationship : relationships) {
    if (relationship.kind == kind && relationship.source_file_path == source_file &&
        relationship.target_text == target_text) {
      return &relationship;
    }
  }
  return nullptr;
}

}  // namespace

TEST_CASE("resolver preserves resolved ambiguous and unresolved calls") {
  const auto relationships = sherpa::RelationshipResolver{}.resolve(relationship_fixture());

  const auto* target =
      find_relationship(relationships, sherpa::RelationshipKind::kCalls, "src/main.cpp", "target");
  REQUIRE(target != nullptr);
  CHECK(target->resolution == sherpa::ResolutionStatus::kResolved);
  CHECK(target->confidence == sherpa::Confidence::kHigh);
  REQUIRE(target->target_symbol.has_value());
  CHECK(target->target_symbol->qualified_name == "target");

  const auto* overloaded = find_relationship(relationships, sherpa::RelationshipKind::kCalls,
                                             "src/main.cpp", "overloaded");
  REQUIRE(overloaded != nullptr);
  CHECK(overloaded->resolution == sherpa::ResolutionStatus::kAmbiguous);
  CHECK(overloaded->candidates.size() == 2);

  const auto* missing =
      find_relationship(relationships, sherpa::RelationshipKind::kCalls, "src/main.cpp", "missing");
  REQUIRE(missing != nullptr);
  CHECK(missing->resolution == sherpa::ResolutionStatus::kUnresolved);
  CHECK_FALSE(missing->target_symbol.has_value());
}

TEST_CASE("frontend does not treat C++ casts as function calls") {
  const auto files = relationship_fixture();
  const auto api = std::ranges::find_if(files, [](const sherpa::IndexedFile& file) {
    return file.file.relative_path == "src/api.cpp";
  });
  REQUIRE(api != files.end());
  CHECK(api->analysis.calls.empty());
}

TEST_CASE("resolver resolves local includes without guessing ambiguous paths") {
  const auto relationships = sherpa::RelationshipResolver{}.resolve(relationship_fixture());

  const auto* api = find_relationship(relationships, sherpa::RelationshipKind::kIncludes,
                                      "src/main.cpp", "api.hpp");
  REQUIRE(api != nullptr);
  CHECK(api->resolution == sherpa::ResolutionStatus::kResolved);
  CHECK(api->confidence == sherpa::Confidence::kMedium);
  REQUIRE(api->target_file_path.has_value());
  CHECK(*api->target_file_path == "include/api.hpp");

  const auto* common = find_relationship(relationships, sherpa::RelationshipKind::kIncludes,
                                         "src/main.cpp", "common.hpp");
  REQUIRE(common != nullptr);
  CHECK(common->resolution == sherpa::ResolutionStatus::kAmbiguous);
  CHECK(common->candidates.size() == 2);

  const auto* system = find_relationship(relationships, sherpa::RelationshipKind::kIncludes,
                                         "src/main.cpp", "vector");
  REQUIRE(system != nullptr);
  CHECK(system->resolution == sherpa::ResolutionStatus::kUnresolved);
  CHECK(system->provenance == "system-include");
}

TEST_CASE("resolver emits deterministic file definition relationships") {
  const auto relationships = sherpa::RelationshipResolver{}.resolve(relationship_fixture());

  const auto* run =
      find_relationship(relationships, sherpa::RelationshipKind::kDefines, "src/main.cpp", "run");
  REQUIRE(run != nullptr);
  CHECK(run->resolution == sherpa::ResolutionStatus::kResolved);
  CHECK(run->confidence == sherpa::Confidence::kHigh);
  REQUIRE(run->target_symbol.has_value());
}
