#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sherpa::api {

inline constexpr std::uint32_t kPluginApiVersion = 1;

enum class Operation {
  kIndex,
  kSymbol,
  kFile,
  kCallers,
  kCallees,
  kImpact,
  kPath,
  kGraph,
};

struct OperationContext {
  Operation operation;
  std::filesystem::path repository_path;
  std::filesystem::path database_path;
};

struct PluginDescriptor {
  std::string id;
  std::string name;
  std::uint32_t api_version{kPluginApiVersion};
};

class Plugin {
 public:
  virtual ~Plugin() = default;

  [[nodiscard]] virtual PluginDescriptor descriptor() const = 0;
  virtual void before_operation(const OperationContext& context);
  virtual void after_operation(const OperationContext& context);
};

struct RegisteredPlugin {
  PluginDescriptor descriptor;
  std::shared_ptr<Plugin> instance;
};

class PluginRegistry {
 public:
  void add(std::shared_ptr<Plugin> plugin);

  [[nodiscard]] const std::vector<RegisteredPlugin>& plugins() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;

 private:
  std::vector<RegisteredPlugin> plugins_;
};

[[nodiscard]] const char* to_string(Operation operation) noexcept;

}  // namespace sherpa::api
