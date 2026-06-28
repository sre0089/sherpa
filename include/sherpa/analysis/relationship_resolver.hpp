#pragma once

#include <vector>

#include "sherpa/domain/indexed_file.hpp"
#include "sherpa/domain/relationship.hpp"

namespace sherpa {

class RelationshipResolver {
 public:
  [[nodiscard]] std::vector<RelationshipRecord> resolve(
      const std::vector<IndexedFile>& files) const;
};

}  // namespace sherpa
