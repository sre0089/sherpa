#include <string_view>

#include "sherpa/api/client.hpp"

int main() {
  sherpa::api::Client client;
  static_cast<void>(client);
  return sherpa::api::kApiVersion == 1 &&
                 std::string_view(sherpa::api::to_string(
                     sherpa::api::ErrorCode::kNotFound)) == "not_found"
             ? 0
             : 1;
}
