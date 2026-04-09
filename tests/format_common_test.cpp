#include "common.h"
#include "format.h"

#include <doctest/doctest.h>

#include <regex>
#include <string>

TEST_CASE("Level to_string returns expected names") {
  CHECK_EQ(std::string(to_string(Level::DEBUG)), "DEBUG");
  CHECK_EQ(std::string(to_string(Level::INFO)), "INFO");
  CHECK_EQ(std::string(to_string(Level::WARN)), "WARN");
  CHECK_EQ(std::string(to_string(Level::ERROR)), "ERROR");
  CHECK_EQ(std::string(to_string(Level::CRITICAL)), "CRITICAL");
}

TEST_CASE("LogEntry preserves explicit constructor fields") {
  LogEntry entry(1'710'000'000'123'456ULL, Level::ERROR, "worker-1", "disk full",
                 "sink.cpp:42");

  CHECK(entry.level == Level::ERROR);
  CHECK_EQ(entry.threadName, "worker-1");
  CHECK_EQ(entry.message, "disk full");
  CHECK_EQ(entry.file, "sink.cpp:42");
}

TEST_CASE("HeaderFormatter expands known tokens and keeps unknown ones") {
  LogEntry entry(1'710'000'000'123'456ULL, Level::ERROR, "worker-1", "disk full",
                 "sink.cpp:42");
  HeaderFormatter formatter("{date} {HMS} {HMSf} [{level}] {thread} {file} {missing}");

  const std::string formatted = formatter.format(entry);

  CHECK_NE(formatted.find("[ERROR] worker-1 sink.cpp:42 {missing}"),
           std::string::npos);
  CHECK(std::regex_search(formatted, std::regex(R"(\d{4}-\d{2}-\d{2})")));
  CHECK(std::regex_search(formatted, std::regex(R"(\d{2}:\d{2}:\d{2})")));
  CHECK(std::regex_search(formatted, std::regex(R"(\d{2}:\d{2}:\d{2}\.\d{6})")));
}
