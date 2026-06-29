#include "sherpa/presentation/file_query_renderer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

TEST_CASE("file query JSON renderer writes definitions and include edges") {
  const sherpa::FileQueryResult result{
      .path = "src/main.cpp",
      .definitions =
          {{
              .kind = "function",
              .name = "run",
              .qualified_name = "run",
              .signature = "run()",
              .file_path = "src/main.cpp",
              .range = {.start_line = 4, .start_column = 1},
          }},
      .incoming_includes = {},
      .outgoing_includes =
          {{
              .source_path = "src/main.cpp",
              .target_path = std::nullopt,
              .target_text = "quoted\"\\\n",
              .resolution = sherpa::ResolutionStatus::kUnresolved,
              .confidence = sherpa::Confidence::kLow,
              .provenance = "parser",
              .evidence = {.start_line = 1, .start_column = 1},
              .candidates = {},
          }},
  };

  std::ostringstream output;
  sherpa::write_file_query_json(output, result);

  REQUIRE(output.str().find("\"schema_version\":1,\"ok\":true") != std::string::npos);
  REQUIRE(output.str().find("\"query\":\"file\"") != std::string::npos);
  REQUIRE(output.str().find("\"path\":\"src/main.cpp\"") != std::string::npos);
  REQUIRE(output.str().find("\"target_text\":\"quoted\\\"\\\\\\n\"") != std::string::npos);
}

TEST_CASE("file query text renderer reports empty sections") {
  const sherpa::FileQueryResult result{
      .path = "include/api.hpp",
      .definitions = {},
      .incoming_includes = {},
      .outgoing_includes = {},
  };

  std::ostringstream output;
  sherpa::write_file_query_text(output, result);

  REQUIRE(output.str() ==
          "File include/api.hpp\n"
          "  Definitions:\n"
          "    None\n"
          "  Incoming includes:\n"
          "    None\n"
          "  Outgoing includes:\n"
          "    None\n");
}
