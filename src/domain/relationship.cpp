#include "sherpa/domain/relationship.hpp"

#include <stdexcept>

namespace sherpa {

const char* to_string(RelationshipKind kind) {
  switch (kind) {
    case RelationshipKind::kDefines:
      return "defines";
    case RelationshipKind::kCalls:
      return "calls";
    case RelationshipKind::kIncludes:
      return "includes";
  }
  throw std::logic_error("unknown relationship kind");
}

const char* to_string(ResolutionStatus status) {
  switch (status) {
    case ResolutionStatus::kResolved:
      return "resolved";
    case ResolutionStatus::kAmbiguous:
      return "ambiguous";
    case ResolutionStatus::kUnresolved:
      return "unresolved";
  }
  throw std::logic_error("unknown resolution status");
}

const char* to_string(Confidence confidence) {
  switch (confidence) {
    case Confidence::kHigh:
      return "high";
    case Confidence::kMedium:
      return "medium";
    case Confidence::kLow:
      return "low";
  }
  throw std::logic_error("unknown confidence");
}

}  // namespace sherpa
