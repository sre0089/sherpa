#pragma once

#include <cstdint>
#include <iosfwd>

namespace sherpa::server {

inline constexpr std::uint32_t kProtocolVersion = 1;

class JsonRpcServer {
 public:
  int run(std::istream& input, std::ostream& output) const;
};

}  // namespace sherpa::server
