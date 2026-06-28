#include "api.hpp"

int target() { return 42; }

int overloaded(int value) { return value; }

int overloaded(double value) { return static_cast<int>(value); }
