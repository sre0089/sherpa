#include "sherpa/presentation/call_query_renderer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

TEST_CASE("call query JSON renderer escapes strings and represents empty results") {
  const sherpa::CallQueryResult result{
      .direction = sherpa::CallQueryDirection::kCallees,
      .symbol =
          {
              .kind = "function",
              .name = "quoted",
              .qualified_name = "quoted\"\\\n",
              .signature = "quoted()",
              .file_path = "src/quoted.cpp",
              .range = {.start_line = 1, .start_column = 1},
          },
      .calls = {},
  };

  std::ostringstream output;
  sherpa::write_call_query_json(output, result);

  REQUIRE(output.str().find("\"query\":\"callees\"") != std::string::npos);
  REQUIRE(output.str().find("\"qualified_name\":\"quoted\\\"\\\\\\n\"") != std::string::npos);
  REQUIRE(output.str().find("\"calls\":[]") != std::string::npos);
}

TEST_CASE("call query text renderer reports an empty result") {
  const sherpa::CallQueryResult result{
      .direction = sherpa::CallQueryDirection::kCallers,
      .symbol =
          {
              .kind = "function",
              .name = "root",
              .qualified_name = "demo::root",
              .signature = "root()",
              .file_path = "src/root.cpp",
              .range = {.start_line = 7, .start_column = 1},
          },
      .calls = {},
  };

  std::ostringstream output;
  sherpa::write_call_query_text(output, result);

  REQUIRE(output.str() == "Callers of demo::root (src/root.cpp:7)\n  None\n");
}
