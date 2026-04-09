#include "sink.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main() {
  const fs::path logPath = fs::temp_directory_path() / "chlog_rotating_file_sink_test.log";
  std::error_code ec;
  fs::remove(logPath, ec);
  fs::remove(logPath.string() + ".1", ec);
  fs::remove(logPath.string() + ".2", ec);

  RotatingFileSink sink(logPath.string(), 1024, 2);
  sink.write("hello", Level::INFO);
  sink.flush();

  std::ifstream input(logPath);
  const std::string content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());

  fs::remove(logPath, ec);
  fs::remove(logPath.string() + ".1", ec);
  fs::remove(logPath.string() + ".2", ec);

  if (content.empty()) {
    std::cerr << "expected log file to contain one message\n";
    return 1;
  }

  if (content != "hello\n") {
    std::cerr << "unexpected log content: " << content << "\n";
    return 1;
  }

  return 0;
}
