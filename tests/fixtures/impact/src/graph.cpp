#include "wrapper.hpp"

int cycle_b();

int leaf() { return 1; }

int middle() { return leaf(); }

int top() { return middle(); }

int cycle_a() { return cycle_b(); }

int cycle_b() { return cycle_a(); }

namespace alpha {
int candidate() { return 1; }
}  // namespace alpha

namespace beta {
int candidate() { return 2; }
}  // namespace beta

int maybe() { return candidate(); }

namespace alpha {
int endpoint() { return 1; }
}  // namespace alpha

namespace beta {
int endpoint() { return 2; }
}  // namespace beta

int bridge() { return alpha::endpoint(); }

int certainty() {
  endpoint();
  return bridge();
}
