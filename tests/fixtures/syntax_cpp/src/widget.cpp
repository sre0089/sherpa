#include "widget.hpp"

namespace demo {

namespace {
int internal_scale(int value) { return value * 2; }
}  // namespace

Widget::Widget() {}

int Widget::compute(int value) const { return internal_scale(value); }

int helper(int value) { return value + 1; }

}  // namespace demo
