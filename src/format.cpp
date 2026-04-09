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

void HeaderFormatter::compile() {
  tokens_.clear();
  std::string::size_type pos = 0;
  while (pos < pattern_.size()) {
    const auto open = pattern_.find('{', pos);
    if (open == std::string::npos) {
      tokens_.push_back(Token{TokenType::LITERAL, pattern_.substr(pos)});
      break;
    }
    if (open > pos) {
      tokens_.push_back(
          Token{TokenType::LITERAL, pattern_.substr(pos, open - pos)});
    }
    const auto close = pattern_.find('}', open);
    if (close == std::string::npos) {
      tokens_.push_back(Token{TokenType::LITERAL, pattern_.substr(open)});
      break;
    }
    const std::string name = pattern_.substr(open + 1, close - open - 1);
    const TokenType type = name_to_type(name);
    if (type != TokenType::LITERAL) {
      tokens_.push_back(Token{type, {}});
    } else {
      tokens_.push_back(
          Token{TokenType::LITERAL, pattern_.substr(open, close - open + 1)});
    }
    pos = close + 1;
  }
}

std::string HeaderFormatter::format(const LogEntry &entry) const {

  std::string out;
  out.reserve(pattern_.size() + 32);
  for (const Token &tok : tokens_) {
    switch (tok.type) {
    case TokenType::LITERAL:
      out += tok.literal;
      break;
    case TokenType::HMS:
      out += format_hms(entry.timestamp);
      break;
    case TokenType::HMSf:
      out += format_hmsf(entry.timestamp);
      break;
    case TokenType::LEVEL:
      out += to_string(entry.level);
      break;
    case TokenType::THREAD:
      out += entry.threadName;
      break;
    case TokenType::FILE:
      out += entry.file;
      break;
    case TokenType::DATE:
      out += format_date(entry.timestamp);
      break;
    }
  }
  return out;
}

HeaderFormatter::TokenType
HeaderFormatter::name_to_type(std::string_view name) {
  if (name == "HMS")
    return TokenType::HMS;
  if (name == "HMSf")
    return TokenType::HMSf;
  if (name == "level")
    return TokenType::LEVEL;
  if (name == "thread")
    return TokenType::THREAD;
  if (name == "file")
    return TokenType::FILE;
  if (name == "date")
    return TokenType::DATE;
  return TokenType::LITERAL;
}
