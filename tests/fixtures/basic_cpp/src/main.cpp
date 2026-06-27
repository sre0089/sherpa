#include "calculator.hpp"

int main() {
  Calculator calculator;
  return calculator.add(20, 22) == 42 ? 0 : 1;
}
