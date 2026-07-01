#include <exception>
#include <iostream>

#ifdef _WIN32
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#endif

#include "server/json_rpc_server.hpp"

int main() {
#ifdef _WIN32
  if (_setmode(_fileno(stdin), _O_BINARY) == -1 ||
      _setmode(_fileno(stdout), _O_BINARY) == -1) {
    std::cerr << "sherpa-server: failed to configure binary stdio\n";
    return 1;
  }
#endif

  try {
    return sherpa::server::JsonRpcServer{}.run(std::cin, std::cout);
  } catch (const std::exception& error) {
    std::cerr << "sherpa-server: " << error.what() << '\n';
    return 1;
  }
}
