#include "sink.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <utility>

namespace fs = std::filesystem;

static_assert(noexcept(std::declval<RotatingFileSink &>().write(
                  std::declval<std::string_view>(), Level::INFO)));
static_assert(noexcept(std::declval<RotatingFileSink &>().flush()));
static_assert(noexcept(std::declval<RotatingFileSink &>().writeChecked(
                  std::declval<std::string_view>(), Level::INFO)));
static_assert(noexcept(std::declval<RotatingFileSink &>().flushChecked()));

TEST_CASE("RotatingFileSink writes plain message content") {
  const fs::path logPath = fs::temp_directory_path() / "chlog_rotating_file_sink_test.log";
  std::error_code ec;
  fs::remove(logPath, ec);
  fs::remove(logPath.string() + ".1", ec);
  fs::remove(logPath.string() + ".2", ec);

  {
    RotatingFileSink sink(logPath.string(), 1024, 2);
    CHECK(sink.writeChecked("hello", Level::INFO));
    CHECK(sink.flushChecked());
    CHECK(sink.lastError().empty());
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

TEST_CASE("RotatingFileSink reports rotation failures") {
  const fs::path dir =
      fs::temp_directory_path() / "chlog_rotating_file_sink_rotation_error_test";
  const fs::path logPath = dir / "sink.log";
  const fs::path blockedBackup(logPath.string() + ".1");

  std::error_code ec;
  fs::remove_all(dir, ec);
  REQUIRE(fs::create_directories(blockedBackup));
  {
    std::ofstream blocker(blockedBackup / "blocker");
    REQUIRE(blocker.good());
    blocker << "blocked";
  }

  std::string error;
  {
    RotatingFileSink sink(logPath.string(), 1, 1);
    CHECK_FALSE(sink.writeChecked("hello", Level::INFO));
    error = sink.lastError();
  }

  fs::remove_all(dir, ec);

  CHECK_NE(error.find("cannot remove"), std::string::npos);
  CHECK_NE(error.find(blockedBackup.string()), std::string::npos);
}
