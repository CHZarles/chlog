#include "chlog.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <regex>
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

TEST_CASE("chlog writes a synchronous console log line") {
  const fs::path capturePath =
      fs::temp_directory_path() / "chlog_sync_log_test.txt";
  std::error_code ec;
  fs::remove(capturePath, ec);

  FILE *capture = std::freopen(capturePath.c_str(), "w", stdout);
  REQUIRE(capture != nullptr);

  auto &logger = chlog::instance();
  logger.level(Level::DEBUG);
  logger.addConsoleSink(false);
  logger.log(Level::INFO, "sync log test", {"demo.cpp", 12, "main"});
  std::fflush(stdout);

  std::ifstream input(capturePath);
  const std::string content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());

  CHECK_NE(content.find("sync log test"), std::string::npos);
  CHECK_NE(content.find("[INFO]"), std::string::npos);
  CHECK_NE(content.find("demo.cpp:12"), std::string::npos);
  CHECK(std::regex_search(content, std::regex(R"(\d{2}:\d{2}:\d{2}\.\d{6})")));
}

TEST_CASE("chlog async queue full falls back to synchronous sink write") {
  const fs::path logPath =
      fs::temp_directory_path() / "chlog_async_queue_full_test.log";
  cleanup_log_path(logPath);

  auto &logger = chlog::instance();
  logger.level(Level::INFO);
  logger.queueConfig(1, 256);
  logger.addRotatingFileSink(logPath.string(), 1024 * 1024, 1);
  logger.start(1s);

  logger.log(Level::INFO, "queued first");
  logger.log(Level::INFO, "sync second");

  const std::string immediate = slurp(logPath);
  CHECK_EQ(immediate.find("queued first"), std::string::npos);
  CHECK_NE(immediate.find("sync second"), std::string::npos);

  logger.stop();

  const std::string finalOutput = slurp(logPath);
  CHECK_NE(finalOutput.find("queued first"), std::string::npos);
  CHECK_NE(finalOutput.find("sync second"), std::string::npos);

  cleanup_log_path(logPath);
}

TEST_CASE("chlog stop drains queued async log entries") {
  const fs::path logPath =
      fs::temp_directory_path() / "chlog_async_stop_drain_test.log";
  cleanup_log_path(logPath);

  auto &logger = chlog::instance();
  logger.level(Level::INFO);
  logger.queueConfig(4, 256);
  logger.addRotatingFileSink(logPath.string(), 1024 * 1024, 1);
  logger.start(1s);

  logger.log(Level::INFO, "drain on stop");

  const std::string immediate = slurp(logPath);
  CHECK_EQ(immediate.find("drain on stop"), std::string::npos);

  logger.stop();

  const std::string finalOutput = slurp(logPath);
  CHECK_NE(finalOutput.find("drain on stop"), std::string::npos);

  cleanup_log_path(logPath);
}
