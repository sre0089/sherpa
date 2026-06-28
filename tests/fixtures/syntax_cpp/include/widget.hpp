#pragma once

#include <string>

namespace demo {

class Forward;

class Widget {
 public:
  Widget();
  int compute(int value) const;
};

struct Options {
  int factor;
};

int helper(int value), secondary(int value);

}  // namespace demo
