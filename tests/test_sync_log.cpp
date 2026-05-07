#include "chlog.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <regex>
#include <future>
#include <string>
#include <thread>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {

std::string slurp(const fs::path &path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

void cleanup_log_path(const fs::path &path) {
  std::error_code ec;
  fs::remove(path, ec);
  for (int i = 1; i <= 3; ++i) {
    fs::remove(path.string() + "." + std::to_string(i), ec);
  }
}

} // namespace

TEST_CASE("chlog global functions compile and call") {
  Logger::instance().level(Level::DEBUG);
  chlog::debug("debug message");
  chlog::info("info message");
  chlog::warn("warn message");
  chlog::error("error message");
  chlog::critical("critical message");
}

TEST_CASE("chlog level configuration") {
  auto &logger = Logger::instance();
  logger.level(Level::WARN);
  // WARN 级别以上才输出
}