#include "sherpa/presentation/impact_renderer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

namespace {

sherpa::ImpactResult empty_impact() {
  return sherpa::ImpactResult{
      .target =
          {
              .kind = sherpa::ImpactNodeKind::kFile,
              .file_path = "include/base.hpp",
              .symbol = std::nullopt,
          },
      .confirmed = {},
      .possible = {},
  };
}

}  // namespace

TEST_CASE("impact text renderer distinguishes confirmed and possible results") {
  std::ostringstream output;
  sherpa::write_impact_text(output, empty_impact());

  REQUIRE(output.str().find("Impact of file include/base.hpp") != std::string::npos);
  REQUIRE(output.str().find("Confirmed impact (0)") != std::string::npos);
  REQUIRE(output.str().find("Possible impact from ambiguous relationships (0)") !=
          std::string::npos);
}

TEST_CASE("impact JSON renderer emits stable empty collections") {
  std::ostringstream output;
  sherpa::write_impact_json(output, empty_impact());

  REQUIRE(output.str().find("\"schema_version\":1,\"ok\":true") != std::string::npos);
  REQUIRE(output.str().find("\"query\":\"impact\"") != std::string::npos);
  REQUIRE(output.str().find("\"kind\":\"file\"") != std::string::npos);
  REQUIRE(output.str().find("\"confirmed\":[]") != std::string::npos);
  REQUIRE(output.str().find("\"possible\":[]") != std::string::npos);
}
