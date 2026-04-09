#include <fmt/format.h>
#include "sink.h"

int main() {
  ConsoleSink sink;

  sink.write("DEBUG message here", Level::DEBUG);
  sink.write("INFO message here", Level::INFO);
  sink.write("WARN message here", Level::WARN);
  sink.write("ERROR message here", Level::ERROR);
  sink.write("CRITICAL message here", Level::CRITICAL);

  fmt::print("formatted: {name} is {age} years old\n",
             fmt::arg("name", "Alice"), fmt::arg("age", 30));
  return 0;
}
