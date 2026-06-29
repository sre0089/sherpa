#include "sherpa/domain/path_query.hpp"

namespace sherpa {

const char* to_string(PathStatus status) {
  switch (status) {
    case PathStatus::kConfirmed:
      return "confirmed";
    case PathStatus::kPossible:
      return "possible";
    case PathStatus::kNotFound:
      return "not_found";
  }
  return "unknown";
}

}  // namespace sherpa
