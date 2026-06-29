#include "sherpa/application/graph_export_service.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <set>
#include <sstream>
#include <string>

#include "sherpa/application/index_service.hpp"
#include "sherpa/presentation/graph_export_renderer.hpp"

#ifndef SHERPA_SOURCE_DIR
#define SHERPA_SOURCE_DIR "."
#endif

namespace {

class ExportTemporaryDirectory {
 public:
  ExportTemporaryDirectory() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ =
        std::filesystem::temp_directory_path() / ("sherpa-export-test-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~ExportTemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

std::filesystem::path fixture(const char* name) {
  return std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / name;
}

std::size_t edge_count(const sherpa::GraphExport& graph, sherpa::RelationshipKind kind) {
  return static_cast<std::size_t>(
      std::ranges::count(graph.edges, kind, &sherpa::GraphExportEdge::kind));
}

}  // namespace

TEST_CASE("graph export contains stable nodes edges and ranked candidates") {
  ExportTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto repository = fixture("impact");
  static_cast<void>(sherpa::IndexService{}.index({repository, database_path}));

  const auto graph = sherpa::GraphExportService{}.build({
      .repository_path = repository,
      .database_path = database_path,
  });

  REQUIRE(graph.schema_version == 1);
  REQUIRE_FALSE(graph.generator_version.empty());
  REQUIRE(graph.nodes.size() == 20);
  REQUIRE(graph.edges.size() == 26);
  REQUIRE(edge_count(graph, sherpa::RelationshipKind::kDefines) == 13);
  REQUIRE(edge_count(graph, sherpa::RelationshipKind::kCalls) == 9);
  REQUIRE(edge_count(graph, sherpa::RelationshipKind::kIncludes) == 4);

  std::set<std::string> node_ids;
  for (const auto& node : graph.nodes) {
    REQUIRE(node_ids.insert(node.id).second);
  }
  std::set<std::string> edge_ids;
  for (const auto& edge : graph.edges) {
    REQUIRE(edge_ids.insert(edge.id).second);
    REQUIRE(node_ids.contains(edge.source_id));
    if (edge.target_id) {
      REQUIRE(node_ids.contains(*edge.target_id));
    }
    for (const auto& candidate : edge.candidates) {
      REQUIRE(node_ids.contains(candidate.node_id));
      REQUIRE_FALSE(candidate.reason.empty());
      REQUIRE(candidate.rank > 0);
    }
  }

  const auto ambiguous_call =
      std::ranges::find_if(graph.edges, [](const sherpa::GraphExportEdge& edge) {
        return edge.kind == sherpa::RelationshipKind::kCalls && edge.target_text == "candidate";
      });
  REQUIRE(ambiguous_call != graph.edges.end());
  REQUIRE(ambiguous_call->resolution == sherpa::ResolutionStatus::kAmbiguous);
  REQUIRE_FALSE(ambiguous_call->target_id);
  REQUIRE(ambiguous_call->candidates.size() == 2);
  REQUIRE(ambiguous_call->candidates[0].rank == 1);
  REQUIRE(ambiguous_call->candidates[1].rank == 2);
}

TEST_CASE("graph export is byte-deterministic for an unchanged index") {
  ExportTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto repository = fixture("impact");
  static_cast<void>(sherpa::IndexService{}.index({repository, database_path}));

  const auto first = sherpa::GraphExportService{}.build({repository, database_path});
  const auto second = sherpa::GraphExportService{}.build({repository, database_path});
  std::ostringstream first_json;
  std::ostringstream second_json;
  sherpa::write_graph_export_json(first_json, first);
  sherpa::write_graph_export_json(second_json, second);

  REQUIRE(first_json.str() == second_json.str());
  REQUIRE(first_json.str().find("\"schema_version\":1") != std::string::npos);
  REQUIRE(first_json.str().find("\"start\":{\"line\":") != std::string::npos);
  REQUIRE(first_json.str().find("\"byte\":") != std::string::npos);
}

TEST_CASE("graph export preserves unresolved targets") {
  ExportTemporaryDirectory temporary;
  const auto database_path = temporary.path() / "index.sqlite";
  const auto repository = fixture("relationships");
  static_cast<void>(sherpa::IndexService{}.index({repository, database_path}));

  const auto graph = sherpa::GraphExportService{}.build(
      {.repository_path = repository, .database_path = database_path});
  const auto missing = std::ranges::find_if(graph.edges, [](const sherpa::GraphExportEdge& edge) {
    return edge.kind == sherpa::RelationshipKind::kCalls && edge.target_text == "missing";
  });

  REQUIRE(missing != graph.edges.end());
  REQUIRE(missing->resolution == sherpa::ResolutionStatus::kUnresolved);
  REQUIRE_FALSE(missing->target_id);
  REQUIRE(missing->candidates.empty());
  REQUIRE_FALSE(missing->provenance.empty());
}
