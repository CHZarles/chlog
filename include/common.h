#pragma once
#include <chrono>
#include <cstdint>
#include <fmt/format.h>

enum class Level : uint32_t {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
  CRITICAL = 4
};

constexpr const char *to_string(Level lvl) {
  switch (lvl) {
  case Level::DEBUG:
    return "DEBUG";
  case Level::INFO:
    return "INFO";
  case Level::WARN:
    return "WARN";
  case Level::ERROR:
    return "ERROR";
  case Level::CRITICAL:
    return "CRITICAL";
  }
  return "UNKNOWN";
}

// source location information
// 被外界调用
struct SourceLocation {
  const char *file = nullptr;
  int line = 0;
  const char *function = nullptr;
};

// 表示日志实体
std::string this_thread_name();
struct LogEntry {
  std::chrono::system_clock::time_point timestamp;
  Level level;
  std::string threadName;
  std::string message;
  std::string file;

  LogEntry(Level lvl, std::string msg, std::string srcFile = {})
      : timestamp(std::chrono::system_clock::now()), level(lvl),
        threadName(this_thread_name()), message(std::move(msg)),
        file(std::move(srcFile)) {}

  LogEntry(uint64_t ts_us, Level lvl, std::string thr, std::string msg,
           std::string srcFile = {})
      : timestamp(std::chrono::microseconds(ts_us)), level(lvl),
        threadName(std::move(thr)), message(std::move(msg)),
        file(std::move(srcFile)) {}
};

[[nodiscard]] inline std::string this_thread_name() {
  char buf[16] = {0};
  const int err = pthread_getname_np(pthread_self(), buf,
                                     sizeof(buf)); // 将线程名写入buffer
  if (err != 0 || buf[0] == '\0')
    return "<unnamed>";
  return std::string(buf);
}
