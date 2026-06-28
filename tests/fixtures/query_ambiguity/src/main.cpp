namespace alpha {
int target() { return 1; }
}  // namespace alpha

namespace beta {
int target() { return 2; }
}  // namespace beta

int run() { return target(); }
