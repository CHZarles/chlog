
#include <fmt/format.h>

int main() {
  fmt::print("nanolog tutorial — stage 1\n");
  fmt::print("formatted: {name} is {age} years old\n",
             fmt::arg("name", "Alice"), fmt::arg("age", 30));
  return 0;
}
