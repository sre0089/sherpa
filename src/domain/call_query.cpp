#include "sherpa/domain/call_query.hpp"

namespace sherpa {

const char* to_string(CallQueryDirection direction) {
  switch (direction) {
    case CallQueryDirection::kCallers:
      return "callers";
    case CallQueryDirection::kCallees:
      return "callees";
  }
  return "unknown";
}

}  // namespace sherpa
