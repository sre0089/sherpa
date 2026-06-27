#include <catch2/catch_test_macros.hpp>

#include "sherpa/util/fingerprint.hpp"

TEST_CASE("fingerprints are stable and input-sensitive") {
  REQUIRE(sherpa::fingerprint("sherpa") == sherpa::fingerprint("sherpa"));
  REQUIRE(sherpa::fingerprint("sherpa") != sherpa::fingerprint("Sherpa"));
  REQUIRE(sherpa::fingerprint("sherpa").size() == 16);
}
