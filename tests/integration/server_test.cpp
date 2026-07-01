#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "server/json_rpc_server.hpp"

namespace {

using Json = nlohmann::json;

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() / ("sherpa-server-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

std::string frame(const Json& message) {
  const auto payload = message.dump();
  return "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n" + payload;
}

std::vector<Json> parse_messages(std::string_view output) {
  std::vector<Json> messages;
  while (!output.empty()) {
    const auto header_end = output.find("\r\n\r\n");
    REQUIRE(header_end != std::string_view::npos);
    const auto header = output.substr(0, header_end);
    constexpr std::string_view prefix{"Content-Length: "};
    REQUIRE(header.starts_with(prefix));
    const auto length = static_cast<std::size_t>(
        std::stoull(std::string(header.substr(prefix.size()))));
    output.remove_prefix(header_end + 4);
    REQUIRE(output.size() >= length);
    messages.push_back(Json::parse(output.substr(0, length)));
    output.remove_prefix(length);
  }
  return messages;
}

Json request(int id, std::string method, Json params = Json::object()) {
  return {
      {"jsonrpc", "2.0"},
      {"id", id},
      {"method", std::move(method)},
      {"params", std::move(params)},
  };
}

Json notification(std::string method, Json params = Json::object()) {
  return {
      {"jsonrpc", "2.0"},
      {"method", std::move(method)},
      {"params", std::move(params)},
  };
}

}  // namespace

TEST_CASE("server enforces initialization and reports protocol capabilities") {
  TemporaryDirectory temporary;
  std::stringstream input;
  std::stringstream output;
  input << frame(request(1, "query/symbol", {{"name", "missing"}}))
        << frame(request(2,
                         "initialize",
                         {
                             {"repository_path", temporary.path().generic_string()},
                             {"database_path",
                              (temporary.path() / "server.sqlite").generic_string()},
                         }))
        << frame(request(3, "workspace/status")) << frame(request(4, "shutdown"))
        << frame(notification("exit"));

  const auto exit_code = sherpa::server::JsonRpcServer{}.run(input, output);
  const auto messages = parse_messages(output.str());

  REQUIRE(exit_code == 0);
  REQUIRE(messages.size() == 4);
  REQUIRE(messages[0]["error"]["code"] == -32001);
  REQUIRE(messages[1]["result"]["protocol_version"] == sherpa::server::kProtocolVersion);
  REQUIRE(messages[1]["result"]["capabilities"]["cancellation"] == true);
  REQUIRE(messages[2]["result"]["initialized"] == true);
  REQUIRE(messages[3]["result"].is_null());
}

TEST_CASE("server indexes and returns existing query JSON contracts") {
  TemporaryDirectory temporary;
  const auto repository =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "relationships";
  const auto database = temporary.path() / "server-query.sqlite";
  std::stringstream input;
  std::stringstream output;
  input << frame(request(1,
                         "initialize",
                         {
                             {"repository_path", repository.generic_string()},
                             {"database_path", database.generic_string()},
                         }))
        << frame(request(2, "workspace/index"))
        << frame(request(3, "query/symbol", {{"name", "run"}}))
        << frame(request(4, "graph/get")) << frame(request(5, "unknown/method"))
        << frame(request(6, "shutdown")) << frame(notification("exit"));

  const auto exit_code = sherpa::server::JsonRpcServer{}.run(input, output);
  const auto messages = parse_messages(output.str());

  REQUIRE(exit_code == 0);
  REQUIRE(messages.size() == 6);
  REQUIRE(messages[1]["result"]["indexed_files"] == 5);
  REQUIRE(messages[2]["result"]["schema_version"] == 1);
  REQUIRE(messages[2]["result"]["query"] == "symbol");
  REQUIRE(messages[2]["result"]["symbol"]["qualified_name"] == "run");
  REQUIRE(messages[3]["result"]["schema_version"] == 1);
  REQUIRE_FALSE(messages[3]["result"]["nodes"].empty());
  REQUIRE(messages[4]["error"]["code"] == -32601);
  REQUIRE(messages[5]["result"].is_null());
}

TEST_CASE("server cancels queued requests by JSON-RPC id") {
  TemporaryDirectory temporary;
  const auto repository =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "relationships";
  const auto database = temporary.path() / "server-cancel.sqlite";
  std::stringstream input;
  std::stringstream output;
  input << frame(request(1,
                         "initialize",
                         {
                             {"repository_path", repository.generic_string()},
                             {"database_path", database.generic_string()},
                         }))
        << frame(request(2, "workspace/index"))
        << frame(request(3, "query/symbol", {{"name", "run"}}))
        << frame(notification("$/cancelRequest", {{"id", 3}}))
        << frame(request(4, "shutdown")) << frame(notification("exit"));

  REQUIRE(sherpa::server::JsonRpcServer{}.run(input, output) == 0);
  const auto messages = parse_messages(output.str());
  REQUIRE(messages.size() == 4);
  REQUIRE(messages[2]["id"] == 3);
  REQUIRE(messages[2]["error"]["code"] == -32800);
  REQUIRE(messages[2]["error"]["data"]["sherpa_code"] == "request_cancelled");
}

TEST_CASE("server returns framed parse errors and rejects exit before shutdown") {
  std::stringstream malformed_input;
  std::stringstream malformed_output;
  malformed_input << "Content-Length: 1\r\n\r\n{";
  REQUIRE(sherpa::server::JsonRpcServer{}.run(malformed_input, malformed_output) == 0);
  const auto malformed_messages = parse_messages(malformed_output.str());
  REQUIRE(malformed_messages.size() == 1);
  REQUIRE(malformed_messages[0]["error"]["code"] == -32700);

  std::stringstream invalid_input;
  std::stringstream invalid_output;
  const auto invalid_payload = Json::object().dump();
  invalid_input << "Content-Length: " << invalid_payload.size() << "\r\r\n\r\r\n"
                << invalid_payload;
  REQUIRE(sherpa::server::JsonRpcServer{}.run(invalid_input, invalid_output) == 0);
  const auto invalid_messages = parse_messages(invalid_output.str());
  REQUIRE(invalid_messages.size() == 1);
  REQUIRE(invalid_messages[0]["id"].is_null());
  REQUIRE(invalid_messages[0]["error"]["code"] == -32600);

  std::stringstream exit_input;
  std::stringstream exit_output;
  exit_input << frame(notification("exit"));
  REQUIRE(sherpa::server::JsonRpcServer{}.run(exit_input, exit_output) == 1);
  REQUIRE(exit_output.str().empty());
}
