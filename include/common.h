#pragma once
#include <cstdint>
#include <fmt/format.h>
#include <string_view>

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
struct SourceLocation {
  const char *file = nullptr;
  int line = 0;
  const char *function = nullptr;
};

[[nodiscard]] inline std::string_view basename_of(std::string_view path) {
  const size_t slash = path.find_last_of("\\/");
  return (slash == std::string_view::npos) ? path : path.substr(slash + 1);
}

[[nodiscard]] inline std::string format_source_location(SourceLocation src) {
  if (!src.file || src.file[0] == '\0' || src.line <= 0)
    return {};
  return fmt::format("{}:{}", basename_of(src.file), src.line);
}

// log entity
[[nodiscard]] inline std::string this_thread_name() {
  char buf[16] = {0};
  const int err = pthread_getname_np(pthread_self(), buf,
                                     sizeof(buf)); // 将线程名写入buffer
  if (err != 0 || buf[0] == '\0')
    return "<unnamed>";
  return std::string(buf);
}
