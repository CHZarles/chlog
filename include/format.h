#pragma once
#include "common.h"
#include <chrono>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

class HeaderFormatter {
public:
  enum class TokenType { LITERAL, HMS, HMSf, LEVEL, THREAD, FILE, DATE };

  struct Token {
    TokenType type;
    std::string literal;
  };

  // 指定日志格式
  explicit HeaderFormatter(std::string pattern = "{HMSf} [{level}] ")
      : pattern_(std::move(pattern)) {
    compile();
  }

  [[nodiscard]] std::string format(const LogEntry &entry) const;

private:
  std::string pattern_;
  std::vector<Token> tokens_;

  static std::string
  format_local_time(const std::chrono::system_clock::time_point &tp,
                    const char *pattern, size_t buffer_size);
  static std::string
  format_date(const std::chrono::system_clock::time_point &tp);

  static std::string
  format_hms(const std::chrono::system_clock::time_point &tp);

  static std::string
  format_hmsf(const std::chrono::system_clock::time_point &tp);

  void compile();

  static TokenType name_to_type(std::string_view name);
};
