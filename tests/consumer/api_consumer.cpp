#include <memory>
#include <string_view>
#include <utility>

#include "sherpa/api/client.hpp"

namespace {

class ConsumerPlugin final : public sherpa::api::Plugin {
 public:
  [[nodiscard]] sherpa::api::PluginDescriptor descriptor() const override {
    return {
        .id = "consumer",
        .name = "Consumer",
        .api_version = sherpa::api::kPluginApiVersion,
    };
  }
};

}  // namespace

int main() {
  sherpa::api::PluginRegistry plugins;
  plugins.add(std::make_shared<ConsumerPlugin>());
  sherpa::api::Client client(std::move(plugins));
  static_cast<void>(client);
  return sherpa::api::kApiVersion == 1 && sherpa::api::kPluginApiVersion == 1 &&
                 std::string_view(sherpa::api::to_string(
                     sherpa::api::ErrorCode::kNotFound)) == "not_found"
             ? 0
             : 1;
}
