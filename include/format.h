#pragma once
#include "common.h"
#include <chrono>
#include <ctime>
#include <string>

#include <fmt/format.h>

class HeaderFormatter {
public:
  struct Options {
    bool show_date = false;
    bool show_time = true;
    bool show_microseconds = true;
    bool show_level = true;
    bool show_thread = false;
    bool show_file = false;
  };

  HeaderFormatter() = default;
  explicit HeaderFormatter(Options options) : options_(options) {}

  std::string format(const LogEntry &entry) const;

private:
  Options options_;

  static std::string
  format_local_time(const std::chrono::system_clock::time_point &tp,
                    const char *pattern, size_t buffer_size);
  static std::string
  format_date(const std::chrono::system_clock::time_point &tp);

  static std::string
  format_hms(const std::chrono::system_clock::time_point &tp);

  static std::string
  format_hmsf(const std::chrono::system_clock::time_point &tp);
};
