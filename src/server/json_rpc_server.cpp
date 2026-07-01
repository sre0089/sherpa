#include "server/json_rpc_server.hpp"

#include <atomic>
#include <charconv>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <istream>
#include <mutex>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

#include "sherpa/api/client.hpp"
#include "sherpa/presentation/call_query_renderer.hpp"
#include "sherpa/presentation/file_query_renderer.hpp"
#include "sherpa/presentation/graph_export_renderer.hpp"
#include "sherpa/presentation/impact_renderer.hpp"
#include "sherpa/presentation/path_renderer.hpp"
#include "sherpa/presentation/symbol_query_renderer.hpp"
#include "sherpa/version.hpp"

namespace sherpa::server {
namespace {

using Json = nlohmann::json;

constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;
constexpr int kInternalError = -32603;
constexpr int kRequestCancelled = -32800;
constexpr int kNotInitialized = -32001;
constexpr int kRepositoryUnavailable = -32010;
constexpr int kIndexUnavailable = -32011;
constexpr int kNotFound = -32012;
constexpr int kAmbiguousSymbol = -32013;
constexpr int kPluginFailure = -32014;
constexpr std::size_t kMaximumMessageSize = 64U * 1024U * 1024U;

class RpcError : public std::runtime_error {
 public:
  RpcError(int code, std::string message, Json data = nullptr)
      : std::runtime_error(std::move(message)), code_(code), data_(std::move(data)) {}

  [[nodiscard]] int code() const noexcept { return code_; }
  [[nodiscard]] const Json& data() const noexcept { return data_; }

 private:
  int code_;
  Json data_;
};

Json error_response(const Json& id, int code, std::string_view message, Json data = nullptr) {
  Json error{
      {"code", code},
      {"message", message},
  };
  if (!data.is_null()) {
    error["data"] = std::move(data);
  }
  return {
      {"jsonrpc", "2.0"},
      {"id", id},
      {"error", std::move(error)},
  };
}

Json success_response(const Json& id, Json result) {
  return {
      {"jsonrpc", "2.0"},
      {"id", id},
      {"result", std::move(result)},
  };
}

Json cancellation_response(const Json& id) {
  return error_response(
      id, kRequestCancelled, "request cancelled", {{"sherpa_code", "request_cancelled"}});
}

void write_message(std::ostream& output, const Json& message, std::mutex& output_mutex) {
  auto payload = message.dump();
  if (payload.size() > kMaximumMessageSize) {
    const auto id = message.is_object() && message.contains("id") ? message.at("id") : Json(nullptr);
    payload = error_response(id, kInternalError, "response exceeds maximum message size").dump();
  }
  std::lock_guard lock(output_mutex);
  output << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
  output.flush();
}

std::optional<std::string> read_payload(std::istream& input) {
  std::string line;
  std::optional<std::size_t> content_length;
  bool read_header = false;

  while (std::getline(input, line)) {
    read_header = true;
    while (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }

    constexpr std::string_view prefix{"Content-Length:"};
    if (line.starts_with(prefix)) {
      auto value = std::string_view(line).substr(prefix.size());
      while (!value.empty() && value.front() == ' ') {
        value.remove_prefix(1);
      }
      std::size_t parsed_length{};
      const auto result =
          std::from_chars(value.data(), value.data() + value.size(), parsed_length);
      if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        throw std::runtime_error("invalid Content-Length header");
      }
      content_length = parsed_length;
    }
  }

  if (!read_header && input.eof()) {
    return std::nullopt;
  }
  if (!content_length.has_value()) {
    throw std::runtime_error("missing Content-Length header");
  }
  if (*content_length > kMaximumMessageSize) {
    throw std::runtime_error("message exceeds maximum size");
  }

  std::string payload(*content_length, '\0');
  input.read(payload.data(), static_cast<std::streamsize>(payload.size()));
  if (input.gcount() != static_cast<std::streamsize>(payload.size())) {
    throw std::runtime_error("incomplete JSON-RPC payload");
  }
  return payload;
}

const Json& request_params(const Json& request) {
  static const Json empty = Json::object();
  if (!request.contains("params")) {
    return empty;
  }
  const auto& params = request.at("params");
  if (!params.is_object()) {
    throw RpcError(kInvalidParams, "params must be an object");
  }
  return params;
}

std::string required_string(const Json& params, std::string_view name) {
  const auto iterator = params.find(std::string(name));
  if (iterator == params.end() || !iterator->is_string() ||
      iterator->get_ref<const std::string&>().empty()) {
    throw RpcError(kInvalidParams, std::string(name) + " must be a non-empty string");
  }
  return iterator->get<std::string>();
}

std::string optional_string(const Json& params, std::string_view name) {
  const auto iterator = params.find(std::string(name));
  if (iterator == params.end()) {
    return {};
  }
  if (!iterator->is_string()) {
    throw RpcError(kInvalidParams, std::string(name) + " must be a string");
  }
  return iterator->get<std::string>();
}

template <typename Writer>
Json render_json(Writer&& writer) {
  std::ostringstream output;
  writer(output);
  return Json::parse(output.str());
}

Json index_result_json(const api::IndexResult& result) {
  return {
      {"repository_path", result.repository_path.generic_string()},
      {"database_path", result.database_path.generic_string()},
      {"indexed_files", result.indexed_files},
      {"added_files", result.added_files},
      {"modified_files", result.modified_files},
      {"unchanged_files", result.unchanged_files},
      {"deleted_files", result.deleted_files},
      {"parsed_files", result.parsed_files},
      {"extracted_symbols", result.extracted_symbols},
      {"extracted_includes", result.extracted_includes},
      {"diagnostics", result.diagnostics},
      {"relationships", result.relationships},
      {"resolved_relationships", result.resolved_relationships},
      {"ambiguous_relationships", result.ambiguous_relationships},
      {"unresolved_relationships", result.unresolved_relationships},
      {"scan_milliseconds", result.scan_milliseconds},
      {"cache_load_milliseconds", result.cache_load_milliseconds},
      {"parse_milliseconds", result.parse_milliseconds},
      {"relationship_milliseconds", result.relationship_milliseconds},
      {"persistence_milliseconds", result.persistence_milliseconds},
      {"total_milliseconds", result.total_milliseconds},
      {"warnings", result.warnings},
  };
}

Json api_error_data(const api::Error& error) {
  Json data{
      {"sherpa_code", api::to_string(error.code())},
  };
  if (!error.plugin_id().empty()) {
    data["plugin_id"] = error.plugin_id();
    data["operation_completed"] = error.operation_completed();
  }
  if (!error.candidates().empty()) {
    data["candidates"] = Json::array();
    for (const auto& candidate : error.candidates()) {
      data["candidates"].push_back({
          {"qualified_name", candidate.qualified_name},
          {"signature", candidate.signature},
          {"file", candidate.file_path},
      });
    }
  }
  return data;
}

RpcError translate_api_error(const api::Error& error) {
  int code = kInternalError;
  switch (error.code()) {
    case api::ErrorCode::kInvalidArgument:
      code = kInvalidParams;
      break;
    case api::ErrorCode::kRepositoryUnavailable:
      code = kRepositoryUnavailable;
      break;
    case api::ErrorCode::kIndexUnavailable:
      code = kIndexUnavailable;
      break;
    case api::ErrorCode::kNotFound:
      code = kNotFound;
      break;
    case api::ErrorCode::kAmbiguousSymbol:
      code = kAmbiguousSymbol;
      break;
    case api::ErrorCode::kPluginFailure:
      code = kPluginFailure;
      break;
    case api::ErrorCode::kInternalFailure:
      code = kInternalError;
      break;
  }
  return RpcError(code, error.what(), api_error_data(error));
}

class Dispatcher {
 public:
  explicit Dispatcher(std::atomic<bool>& shutdown_seen) : shutdown_seen_(shutdown_seen) {}

  Json dispatch(const Json& request) {
    validate_request(request);
    const auto method = request.at("method").get<std::string>();
    const auto& params = request_params(request);

    if (shutdown_ && method != "shutdown") {
      throw RpcError(kInvalidRequest, "server is shutting down");
    }
    if (method == "initialize") {
      return initialize(params);
    }
    if (method == "shutdown") {
      shutdown_ = true;
      shutdown_seen_.store(true);
      return nullptr;
    }
    if (method == "workspace/status") {
      return status();
    }

    require_initialized();
    if (method == "workspace/index") {
      return index();
    }
    if (method == "query/symbol") {
      return symbol(params);
    }
    if (method == "query/file") {
      return file(params);
    }
    if (method == "query/callers") {
      return calls(params, true);
    }
    if (method == "query/callees") {
      return calls(params, false);
    }
    if (method == "query/impact") {
      return impact(params);
    }
    if (method == "query/path") {
      return path(params);
    }
    if (method == "graph/get") {
      return graph();
    }
    throw RpcError(kMethodNotFound, "method not found: " + method);
  }

 private:
  static void validate_request(const Json& request) {
    if (!request.is_object() || !request.contains("jsonrpc") ||
        !request.at("jsonrpc").is_string() || request.at("jsonrpc") != "2.0" ||
        !request.contains("method") || !request.at("method").is_string()) {
      throw RpcError(kInvalidRequest, "invalid JSON-RPC request");
    }
    if (request.contains("id") && !request.at("id").is_string() &&
        !request.at("id").is_number_integer() && !request.at("id").is_number_unsigned()) {
      throw RpcError(kInvalidRequest, "request id must be a string or integer");
    }
  }

  Json initialize(const Json& params) {
    if (repository_.has_value()) {
      throw RpcError(kInvalidRequest, "server is already initialized");
    }
    repository_ = api::Repository{
        .path = required_string(params, "repository_path"),
        .database_path = optional_string(params, "database_path"),
    };
    return {
        {"protocol_version", kProtocolVersion},
        {"server_version", SHERPA_VERSION},
        {"capabilities",
         {
             {"cancellation", true},
             {"serial_requests", true},
             {"methods",
              {
                  "workspace/status",
                  "workspace/index",
                  "query/symbol",
                  "query/file",
                  "query/callers",
                  "query/callees",
                  "query/impact",
                  "query/path",
                  "graph/get",
              }},
         }},
    };
  }

  Json status() const {
    Json result{
        {"initialized", repository_.has_value()},
    };
    if (repository_.has_value()) {
      result["repository_path"] = repository_->path.generic_string();
      result["database_path"] = repository_->database_path.generic_string();
    }
    return result;
  }

  void require_initialized() const {
    if (!repository_.has_value()) {
      throw RpcError(
          kNotInitialized, "server is not initialized", {{"sherpa_code", "not_initialized"}});
    }
  }

  Json index() const {
    const auto result = client_.index({
        .repository_path = repository_->path,
        .database_path = repository_->database_path,
    });
    return index_result_json(result);
  }

  Json symbol(const Json& params) const {
    const auto result = client_.symbol({
        .repository = *repository_,
        .symbol =
            {
                .name = required_string(params, "name"),
                .signature = optional_string(params, "signature"),
                .file_path = optional_string(params, "file"),
            },
    });
    return render_json([&result](std::ostream& output) {
      write_symbol_query_json(output, result);
    });
  }

  Json file(const Json& params) const {
    const auto result = client_.file({
        .repository = *repository_,
        .path = required_string(params, "path"),
    });
    return render_json(
        [&result](std::ostream& output) { write_file_query_json(output, result); });
  }

  Json calls(const Json& params, bool callers) const {
    const api::SymbolRequest request{
        .repository = *repository_,
        .symbol =
            {
                .name = required_string(params, "name"),
                .signature = optional_string(params, "signature"),
                .file_path = optional_string(params, "file"),
            },
    };
    const auto result = callers ? client_.callers(request) : client_.callees(request);
    return render_json(
        [&result](std::ostream& output) { write_call_query_json(output, result); });
  }

  Json impact(const Json& params) const {
    const auto result = client_.impact({
        .repository = *repository_,
        .target = required_string(params, "target"),
        .signature = optional_string(params, "signature"),
        .file_path = optional_string(params, "file"),
    });
    return render_json([&result](std::ostream& output) { write_impact_json(output, result); });
  }

  Json path(const Json& params) const {
    const auto result = client_.path({
        .repository = *repository_,
        .source =
            {
                .name = required_string(params, "source"),
                .signature = optional_string(params, "source_signature"),
                .file_path = optional_string(params, "source_file"),
            },
        .target =
            {
                .name = required_string(params, "target"),
                .signature = optional_string(params, "target_signature"),
                .file_path = optional_string(params, "target_file"),
            },
    });
    return render_json([&result](std::ostream& output) { write_path_json(output, result); });
  }

  Json graph() const {
    const auto result = client_.graph(*repository_);
    return render_json(
        [&result](std::ostream& output) { write_graph_export_json(output, result); });
  }

  api::Client client_;
  std::optional<api::Repository> repository_;
  bool shutdown_{};
  std::atomic<bool>& shutdown_seen_;
};

std::string request_key(const Json& id) { return id.dump(); }

Json request_id_or_null(const Json& request) {
  if (request.is_object() && request.contains("id")) {
    return request.at("id");
  }
  return nullptr;
}

bool is_notification(const Json& request) {
  return request.is_object() && request.contains("jsonrpc") &&
         request.at("jsonrpc").is_string() && request.at("jsonrpc") == "2.0" &&
         request.contains("method") && request.at("method").is_string() && !request.contains("id");
}

}  // namespace

int JsonRpcServer::run(std::istream& input, std::ostream& output) const {
  std::deque<Json> requests;
  std::unordered_set<std::string> active_requests;
  std::unordered_set<std::string> cancelled_requests;
  std::mutex state_mutex;
  std::mutex output_mutex;
  std::condition_variable request_available;
  bool input_closed = false;
  bool exit_received = false;
  std::atomic<bool> shutdown_seen{false};

  std::thread worker([&] {
    Dispatcher dispatcher(shutdown_seen);
    while (true) {
      Json request;
      {
        std::unique_lock lock(state_mutex);
        request_available.wait(lock, [&] { return input_closed || !requests.empty(); });
        if (requests.empty()) {
          return;
        }
        request = std::move(requests.front());
        requests.pop_front();
      }

      const auto id = request_id_or_null(request);
      const auto has_id = !id.is_null();
      const auto notification = is_notification(request);
      const auto key = has_id ? request_key(id) : std::string{};
      bool cancelled = false;
      if (has_id) {
        std::lock_guard lock(state_mutex);
        cancelled = cancelled_requests.contains(key);
      }

      std::optional<Json> response;
      if (cancelled) {
        response = cancellation_response(id);
      } else {
        try {
          const auto result = dispatcher.dispatch(request);
          if (has_id) {
            response = success_response(id, result);
          }
        } catch (const api::Error& error) {
          if (has_id) {
            const auto translated = translate_api_error(error);
            response =
                error_response(id, translated.code(), translated.what(), translated.data());
          }
        } catch (const RpcError& error) {
          if (has_id || !notification) {
            response = error_response(id, error.code(), error.what(), error.data());
          }
        } catch (const std::exception& error) {
          if (has_id || !notification) {
            response = error_response(id, kInternalError, error.what());
          }
        }
      }

      if (has_id) {
        std::lock_guard lock(state_mutex);
        if (cancelled_requests.contains(key)) {
          response = cancellation_response(id);
        }
        active_requests.erase(key);
        cancelled_requests.erase(key);
      }
      if (response.has_value()) {
        write_message(output, *response, output_mutex);
      }
    }
  });

  while (true) {
    std::optional<std::string> payload;
    try {
      payload = read_payload(input);
    } catch (const std::exception& error) {
      write_message(output, error_response(nullptr, kParseError, error.what()), output_mutex);
      break;
    }
    if (!payload.has_value()) {
      break;
    }

    Json request;
    try {
      request = Json::parse(*payload);
    } catch (const Json::parse_error&) {
      write_message(output, error_response(nullptr, kParseError, "invalid JSON"), output_mutex);
      continue;
    }

    const auto is_method = request.is_object() && request.contains("method") &&
                           request.at("method").is_string();
    const auto method = is_method ? request.at("method").get<std::string>() : std::string{};
    const auto notification = is_notification(request);
    if (notification && method == "$/cancelRequest") {
      if (request.contains("params") && request.at("params").is_object() &&
          request.at("params").contains("id")) {
        const auto key = request_key(request.at("params").at("id"));
        std::lock_guard lock(state_mutex);
        if (active_requests.contains(key)) {
          cancelled_requests.insert(key);
        }
      }
      continue;
    }
    if (notification && method == "exit") {
      exit_received = true;
      break;
    }

    {
      std::lock_guard lock(state_mutex);
      if (request.contains("id") && !request.at("id").is_null()) {
        active_requests.insert(request_key(request.at("id")));
      }
      requests.push_back(std::move(request));
    }
    request_available.notify_one();
  }

  {
    std::lock_guard lock(state_mutex);
    input_closed = true;
  }
  request_available.notify_one();
  worker.join();

  return exit_received && !shutdown_seen.load() ? 1 : 0;
}

}  // namespace sherpa::server
