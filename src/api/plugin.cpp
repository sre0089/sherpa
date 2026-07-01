#include "sherpa/api/plugin.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sherpa::api {
namespace {

bool is_valid_plugin_id(std::string_view id) {
  const auto is_letter_or_digit = [](char value) {
    return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9');
  };
  if (id.empty() || !is_letter_or_digit(id.front())) {
    return false;
  }

  return std::all_of(id.begin(), id.end(), [&is_letter_or_digit](char value) {
    return is_letter_or_digit(value) || value == '.' || value == '_' || value == '-';
  });
}

}  // namespace

void Plugin::before_operation(const OperationContext&) {}

void Plugin::after_operation(const OperationContext&) {}

void PluginRegistry::add(std::shared_ptr<Plugin> plugin) {
  if (!plugin) {
    throw std::invalid_argument("plugin instance is required");
  }

  auto descriptor = plugin->descriptor();
  if (!is_valid_plugin_id(descriptor.id)) {
    throw std::invalid_argument(
        "plugin id must start with a lowercase ASCII letter or digit and contain only lowercase "
        "ASCII letters, digits, '.', '_', or '-'");
  }
  if (descriptor.name.empty()) {
    throw std::invalid_argument("plugin name is required");
  }
  if (descriptor.api_version != kPluginApiVersion) {
    throw std::invalid_argument("plugin '" + descriptor.id + "' requires unsupported plugin API " +
                                std::to_string(descriptor.api_version));
  }

  const auto duplicate = std::find_if(
      plugins_.begin(), plugins_.end(), [&descriptor](const RegisteredPlugin& registered) {
        return registered.descriptor.id == descriptor.id;
      });
  if (duplicate != plugins_.end()) {
    throw std::invalid_argument("plugin id '" + descriptor.id + "' is already registered");
  }

  plugins_.push_back({
      .descriptor = std::move(descriptor),
      .instance = std::move(plugin),
  });
}

const std::vector<RegisteredPlugin>& PluginRegistry::plugins() const noexcept { return plugins_; }

bool PluginRegistry::empty() const noexcept { return plugins_.empty(); }

std::size_t PluginRegistry::size() const noexcept { return plugins_.size(); }

const char* to_string(Operation operation) noexcept {
  switch (operation) {
    case Operation::kIndex:
      return "index";
    case Operation::kSymbol:
      return "symbol";
    case Operation::kFile:
      return "file";
    case Operation::kCallers:
      return "callers";
    case Operation::kCallees:
      return "callees";
    case Operation::kImpact:
      return "impact";
    case Operation::kPath:
      return "path";
    case Operation::kGraph:
      return "graph";
  }
  return "unknown";
}

}  // namespace sherpa::api
