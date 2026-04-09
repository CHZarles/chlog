#include "format.h"

std::string HeaderFormatter::format_local_time(
    const std::chrono::system_clock::time_point &tp, const char *pattern,
    size_t buffer_size) {
  const time_t sec = std::chrono::system_clock::to_time_t(tp);
  std::tm tm_buf{};
  localtime_r(&sec, &tm_buf);

  std::string buffer(buffer_size, '\0');
  const size_t len =
      std::strftime(buffer.data(), buffer.size(), pattern, &tm_buf);
  buffer.resize(len);
  return buffer;
}

std::string
HeaderFormatter::format_date(const std::chrono::system_clock::time_point &tp) {
  return format_local_time(tp, "%Y-%m-%d", 11);
}

std::string
HeaderFormatter::format_hms(const std::chrono::system_clock::time_point &tp) {
  return format_local_time(tp, "%H:%M:%S", 9);
}

std::string
HeaderFormatter::format_hmsf(const std::chrono::system_clock::time_point &tp) {
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      tp.time_since_epoch()) %
                  1'000'000;
  return fmt::format("{}.{:06d}", format_local_time(tp, "%H:%M:%S", 9),
                     static_cast<int>(us.count()));
}

std::string HeaderFormatter::format(const LogEntry &entry) const {
  std::string out;

  auto append_field = [&out](std::string_view field) {
    if (field.empty()) {
      return;
    }
    if (!out.empty()) {
      out += ' ';
    }
    out += field;
  };

  if (options_.show_date) {
    append_field(format_date(entry.timestamp));
  }

  if (options_.show_time) {
    append_field(options_.show_microseconds ? format_hmsf(entry.timestamp)
                                            : format_hms(entry.timestamp));
  }

  if (options_.show_level) {
    append_field(fmt::format("[{}]", to_string(entry.level)));
  }

  if (options_.show_thread) {
    append_field(entry.threadName);
  }

  if (options_.show_file) {
    append_field(entry.file);
  }

  return out;
}
