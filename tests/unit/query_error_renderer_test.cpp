#include "sherpa/presentation/query_error_renderer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

TEST_CASE("query error JSON renderer emits the versioned error contract") {
  const std::vector<sherpa::QuerySymbol> candidates{
      {
          .kind = "function",
          .name = "run",
          .qualified_name = "demo::run",
          .signature = "run()",
          .file_path = "src/demo.cpp",
          .range = {.start_line = 4, .start_column = 2},
      },
  };

  std::ostringstream output;
  sherpa::write_query_error_json(output, "ambiguous_symbol", "quoted\" error", candidates);

  REQUIRE(output.str().find("\"schema_version\":1") != std::string::npos);
  REQUIRE(output.str().find("\"ok\":false") != std::string::npos);
  REQUIRE(output.str().find("\"code\":\"ambiguous_symbol\"") != std::string::npos);
  REQUIRE(output.str().find("\"message\":\"quoted\\\" error\"") != std::string::npos);
  REQUIRE(output.str().find("\"qualified_name\":\"demo::run\"") != std::string::npos);
}
