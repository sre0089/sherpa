#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "sherpa/api/client.hpp"

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ =
        std::filesystem::temp_directory_path() / ("sherpa-public-api-" + std::to_string(suffix));
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

class RecordingPlugin final : public sherpa::api::Plugin {
 public:
  RecordingPlugin(std::string id,
                  std::vector<std::string>& events,
                  bool fail_before = false,
                  bool fail_after = false)
      : id_(std::move(id)),
        events_(events),
        fail_before_(fail_before),
        fail_after_(fail_after) {}

  [[nodiscard]] sherpa::api::PluginDescriptor descriptor() const override {
    return {
        .id = id_,
        .name = id_,
        .api_version = sherpa::api::kPluginApiVersion,
    };
  }

  void before_operation(const sherpa::api::OperationContext& context) override {
    events_.push_back(id_ + ":before:" + sherpa::api::to_string(context.operation));
    if (fail_before_) {
      throw std::runtime_error("before failed");
    }
  }

  void after_operation(const sherpa::api::OperationContext& context) override {
    events_.push_back(id_ + ":after:" + sherpa::api::to_string(context.operation));
    if (fail_after_) {
      throw std::runtime_error("after failed");
    }
  }

 private:
  std::string id_;
  std::vector<std::string>& events_;
  bool fail_before_;
  bool fail_after_;
};

}  // namespace

TEST_CASE("public API indexes and queries without exposing infrastructure") {
  TemporaryDirectory temporary;
  const auto repository =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "relationships";
  const auto database = temporary.path() / "api.sqlite";
  const sherpa::api::Client client;

  const auto indexed = client.index({
      .repository_path = repository,
      .database_path = database,
  });
  REQUIRE(indexed.indexed_files == 5);
  REQUIRE(indexed.parsed_files == 5);

  const sherpa::api::Repository indexed_repository{
      .path = repository,
      .database_path = database,
  };
  const auto symbol = client.symbol({
      .repository = indexed_repository,
      .symbol =
          {
              .name = "run",
              .signature = "",
              .file_path = "",
          },
  });
  REQUIRE(symbol.symbol.qualified_name == "run");

  const auto calls = client.callees({
      .repository = indexed_repository,
      .symbol =
          {
              .name = "run",
              .signature = "",
              .file_path = "",
          },
  });
  REQUIRE_FALSE(calls.calls.empty());

  const auto graph = client.graph(indexed_repository);
  REQUIRE_FALSE(graph.nodes.empty());
  REQUIRE_FALSE(graph.edges.empty());
}

TEST_CASE("public API reports stable error categories and ambiguity candidates") {
  TemporaryDirectory temporary;
  const auto repository =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "relationships";
  const auto database = temporary.path() / "api-errors.sqlite";
  const sherpa::api::Client client;
  static_cast<void>(client.index({
      .repository_path = repository,
      .database_path = database,
  }));
  const sherpa::api::Repository indexed_repository{
      .path = repository,
      .database_path = database,
  };

  try {
    static_cast<void>(client.symbol({
        .repository = indexed_repository,
        .symbol =
            {
                .name = "overloaded",
                .signature = "",
                .file_path = "",
            },
    }));
    FAIL("expected an ambiguous symbol error");
  } catch (const sherpa::api::Error& error) {
    REQUIRE(error.code() == sherpa::api::ErrorCode::kAmbiguousSymbol);
    REQUIRE(error.candidates().size() == 2);
  }

  try {
    static_cast<void>(client.symbol({
        .repository = indexed_repository,
        .symbol =
            {
                .name = "missing",
                .signature = "",
                .file_path = "",
            },
    }));
    FAIL("expected a not-found error");
  } catch (const sherpa::api::Error& error) {
    REQUIRE(error.code() == sherpa::api::ErrorCode::kNotFound);
    REQUIRE(error.candidates().empty());
  }
}

TEST_CASE("public API invokes plugins deterministically around successful operations") {
  TemporaryDirectory temporary;
  const auto repository =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "basic_cpp";
  const auto database = temporary.path() / "plugin-order.sqlite";
  std::vector<std::string> events;
  sherpa::api::PluginRegistry plugins;
  plugins.add(std::make_shared<RecordingPlugin>("first", events));
  plugins.add(std::make_shared<RecordingPlugin>("second", events));
  const sherpa::api::Client client(std::move(plugins));

  static_cast<void>(client.index({
      .repository_path = repository,
      .database_path = database,
  }));

  REQUIRE(events == std::vector<std::string>{
                        "first:before:index",
                        "second:before:index",
                        "first:after:index",
                        "second:after:index",
                    });
}

TEST_CASE("public API does not invoke after hooks when core work fails") {
  TemporaryDirectory temporary;
  std::vector<std::string> events;
  sherpa::api::PluginRegistry plugins;
  plugins.add(std::make_shared<RecordingPlugin>("observer", events));
  const sherpa::api::Client client(std::move(plugins));

  REQUIRE_THROWS_AS(
      client.graph({
          .path = temporary.path(),
          .database_path = temporary.path() / "missing.sqlite",
      }),
      sherpa::api::Error);
  REQUIRE(events == std::vector<std::string>{"observer:before:graph"});
}

TEST_CASE("public API reports whether plugin failures precede or follow core work") {
  TemporaryDirectory temporary;
  const auto repository =
      std::filesystem::path(SHERPA_SOURCE_DIR) / "tests" / "fixtures" / "basic_cpp";
  std::vector<std::string> events;

  const auto blocked_database = temporary.path() / "blocked.sqlite";
  sherpa::api::PluginRegistry blocking_plugins;
  blocking_plugins.add(
      std::make_shared<RecordingPlugin>("blocking", events, true, false));
  const sherpa::api::Client blocked_client(std::move(blocking_plugins));
  try {
    static_cast<void>(blocked_client.index({
        .repository_path = repository,
        .database_path = blocked_database,
    }));
    FAIL("expected a plugin failure");
  } catch (const sherpa::api::Error& error) {
    REQUIRE(error.code() == sherpa::api::ErrorCode::kPluginFailure);
    REQUIRE(error.plugin_id() == "blocking");
    REQUIRE_FALSE(error.operation_completed());
  }
  REQUIRE_FALSE(std::filesystem::exists(blocked_database));

  const auto completed_database = temporary.path() / "completed.sqlite";
  sherpa::api::PluginRegistry failing_plugins;
  failing_plugins.add(
      std::make_shared<RecordingPlugin>("failing", events, false, true));
  const sherpa::api::Client failing_client(std::move(failing_plugins));
  try {
    static_cast<void>(failing_client.index({
        .repository_path = repository,
        .database_path = completed_database,
    }));
    FAIL("expected a plugin failure");
  } catch (const sherpa::api::Error& error) {
    REQUIRE(error.code() == sherpa::api::ErrorCode::kPluginFailure);
    REQUIRE(error.plugin_id() == "failing");
    REQUIRE(error.operation_completed());
  }

  const sherpa::api::Client plain_client;
  const auto graph = plain_client.graph({
      .path = repository,
      .database_path = completed_database,
  });
  REQUIRE_FALSE(graph.nodes.empty());
}
