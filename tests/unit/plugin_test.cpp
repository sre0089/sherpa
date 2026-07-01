#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "sherpa/api/plugin.hpp"

namespace {

class TestPlugin final : public sherpa::api::Plugin {
 public:
  explicit TestPlugin(sherpa::api::PluginDescriptor descriptor)
      : descriptor_(std::move(descriptor)) {}

  [[nodiscard]] sherpa::api::PluginDescriptor descriptor() const override {
    return descriptor_;
  }

 private:
  sherpa::api::PluginDescriptor descriptor_;
};

}  // namespace

TEST_CASE("plugin registry validates identity compatibility and uniqueness") {
  sherpa::api::PluginRegistry registry;

  REQUIRE_THROWS_AS(registry.add(nullptr), std::invalid_argument);
  REQUIRE_THROWS_AS(
      registry.add(std::make_shared<TestPlugin>(sherpa::api::PluginDescriptor{
          .id = "Invalid ID",
          .name = "Invalid",
          .api_version = sherpa::api::kPluginApiVersion,
      })),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      registry.add(std::make_shared<TestPlugin>(sherpa::api::PluginDescriptor{
          .id = "-invalid",
          .name = "Invalid",
          .api_version = sherpa::api::kPluginApiVersion,
      })),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      registry.add(std::make_shared<TestPlugin>(sherpa::api::PluginDescriptor{
          .id = "missing-name",
          .name = "",
          .api_version = sherpa::api::kPluginApiVersion,
      })),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      registry.add(std::make_shared<TestPlugin>(sherpa::api::PluginDescriptor{
          .id = "future",
          .name = "Future",
          .api_version = sherpa::api::kPluginApiVersion + 1,
      })),
      std::invalid_argument);

  registry.add(std::make_shared<TestPlugin>(sherpa::api::PluginDescriptor{
      .id = "example.telemetry",
      .name = "Telemetry",
      .api_version = sherpa::api::kPluginApiVersion,
  }));
  REQUIRE(registry.size() == 1);
  REQUIRE_FALSE(registry.empty());
  REQUIRE(registry.plugins().front().descriptor.id == "example.telemetry");

  REQUIRE_THROWS_AS(
      registry.add(std::make_shared<TestPlugin>(sherpa::api::PluginDescriptor{
          .id = "example.telemetry",
          .name = "Duplicate",
          .api_version = sherpa::api::kPluginApiVersion,
      })),
      std::invalid_argument);
}

TEST_CASE("plugin operation names are stable") {
  REQUIRE(std::string(sherpa::api::to_string(sherpa::api::Operation::kIndex)) == "index");
  REQUIRE(std::string(sherpa::api::to_string(sherpa::api::Operation::kCallees)) == "callees");
  REQUIRE(std::string(sherpa::api::to_string(sherpa::api::Operation::kGraph)) == "graph");
}
