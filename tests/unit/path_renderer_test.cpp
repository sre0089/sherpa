#include "sherpa/presentation/path_renderer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

namespace {

sherpa::QuerySymbol symbol(std::string name) {
  return sherpa::QuerySymbol{
      .kind = "function",
      .name = name,
      .qualified_name = name,
      .signature = name + "()",
      .file_path = "src/" + name + ".cpp",
      .range = {.start_line = 1, .start_column = 1},
  };
}

}  // namespace

TEST_CASE("path text renderer reports disconnected symbols") {
  const sherpa::PathResult result{
      .source = symbol("source"),
      .target = symbol("target"),
      .status = sherpa::PathStatus::kNotFound,
      .steps = {},
  };
  std::ostringstream output;
  sherpa::write_path_text(output, result);

  REQUIRE(output.str().find("Path from source to target: not_found") != std::string::npos);
  REQUIRE(output.str().find("No directed call path found") != std::string::npos);
}

TEST_CASE("path JSON renderer preserves possible-step evidence") {
  const sherpa::PathResult result{
      .source = symbol("source"),
      .target = symbol("target"),
      .status = sherpa::PathStatus::kPossible,
      .steps =
          {
              {
                  .source = symbol("source"),
                  .target = symbol("target"),
                  .resolution = sherpa::ResolutionStatus::kAmbiguous,
                  .confidence = sherpa::Confidence::kMedium,
                  .provenance = "test",
                  .evidence = {.start_line = 4, .start_column = 3},
              },
          },
  };
  std::ostringstream output;
  sherpa::write_path_json(output, result);

  REQUIRE(output.str().find("\"query\":\"path\"") != std::string::npos);
  REQUIRE(output.str().find("\"status\":\"possible\"") != std::string::npos);
  REQUIRE(output.str().find("\"resolution\":\"ambiguous\"") != std::string::npos);
}
