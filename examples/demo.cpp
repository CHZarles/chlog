#include "sink.h"
#include <fmt/format.h>
#include <filesystem>

namespace fs = std::filesystem;

int main() {
  ConsoleSink sink;

  sink.write("DEBUG message here", Level::DEBUG);
  sink.write("INFO message here", Level::INFO);
  sink.write("WARN message here", Level::WARN);
  sink.write("ERROR message here", Level::ERROR);
  sink.write("CRITICAL message here", Level::CRITICAL);

  fmt::print("formatted: {name} is {age} years old\n",
             fmt::arg("name", "Alice"), fmt::arg("age", 30));

  const fs::path logPath = fs::current_path() / "demo.log";
  std::error_code ec;
  fs::remove(logPath, ec);
  fs::remove(logPath.string() + ".1", ec);
  fs::remove(logPath.string() + ".2", ec);

  RotatingFileSink rotatingSink(logPath.string(), 48, 2);
  rotatingSink.write("one", Level::INFO);
  rotatingSink.write("two", Level::WARN);
  rotatingSink.write("three", Level::ERROR);
  rotatingSink.write("four", Level::DEBUG);
  rotatingSink.write("five", Level::INFO);
  rotatingSink.write("six", Level::WARN);
  rotatingSink.write("seven", Level::ERROR);
  rotatingSink.write("eight", Level::DEBUG);
  rotatingSink.flush();

  fmt::print("generated log files:\n");
  for (const auto &path :
       {logPath, fs::path(logPath.string() + ".1"), fs::path(logPath.string() + ".2")}) {
    if (fs::exists(path)) {
      fmt::print("  {}\n", path.filename().string());
    }
  }

  return 0;
}
