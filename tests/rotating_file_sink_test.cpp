#include "sink.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

TEST_CASE("RotatingFileSink writes plain message content") {
  const fs::path logPath = fs::temp_directory_path() / "chlog_rotating_file_sink_test.log";
  std::error_code ec;
  fs::remove(logPath, ec);
  fs::remove(logPath.string() + ".1", ec);
  fs::remove(logPath.string() + ".2", ec);

  {
    RotatingFileSink sink(logPath.string(), 1024, 2);
    sink.write("hello", Level::INFO);
    sink.flush();
  }

  std::ifstream input(logPath);
  const std::string content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());

  fs::remove(logPath, ec);
  fs::remove(logPath.string() + ".1", ec);
  fs::remove(logPath.string() + ".2", ec);

  CHECK_FALSE(content.empty());
  CHECK_EQ(content, "hello\n");
}
