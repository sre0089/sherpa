#include "sherpa/presentation/symbol_query_renderer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

TEST_CASE("symbol query JSON renderer includes query kind and counts") {
  const sherpa::SymbolQueryResult result{
      .symbol =
          {
              .kind = "function",
              .name = "quoted",
              .qualified_name = "quoted\"\\\n",
              .signature = "quoted()",
              .file_path = "src/quoted.cpp",
              .range = {.start_line = 1, .start_column = 2},
          },
      .callers = {.resolved = 1, .ambiguous = 2, .unresolved = 0},
      .callees = {.resolved = 3, .ambiguous = 4, .unresolved = 5},
  };

  std::ostringstream output;
  sherpa::write_symbol_query_json(output, result);

  REQUIRE(output.str().find("\"schema_version\":1,\"ok\":true") != std::string::npos);
  REQUIRE(output.str().find("\"query\":\"symbol\"") != std::string::npos);
  REQUIRE(output.str().find("\"qualified_name\":\"quoted\\\"\\\\\\n\"") != std::string::npos);
  REQUIRE(output.str().find("\"callees\":{\"resolved\":3,\"ambiguous\":4,\"unresolved\":5}") !=
          std::string::npos);
}

TEST_CASE("symbol query text renderer reports counts") {
  const sherpa::SymbolQueryResult result{
      .symbol =
          {
              .kind = "function",
              .name = "root",
              .qualified_name = "demo::root",
              .signature = "root()",
              .file_path = "src/root.cpp",
              .range = {.start_line = 7, .start_column = 1},
          },
      .callers = {.resolved = 1, .ambiguous = 0, .unresolved = 0},
      .callees = {.resolved = 2, .ambiguous = 1, .unresolved = 1},
  };

  std::ostringstream output;
  sherpa::write_symbol_query_text(output, result);

  REQUIRE(output.str() ==
          "Symbol demo::root\n"
          "  Kind: function\n"
          "  Signature: root()\n"
          "  Defined at: src/root.cpp:7:1\n"
          "  Callers: 1 resolved, 0 ambiguous\n"
          "  Callees: 2 resolved, 1 ambiguous, 1 unresolved\n");
}
