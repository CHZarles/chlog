#include "chlog.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>

namespace fs = std::filesystem;

TEST_CASE("chlog writes a synchronous console log line") {
  const fs::path capturePath = fs::temp_directory_path() / "chlog_sync_log_test.txt";
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
