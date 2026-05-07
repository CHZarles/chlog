#pragma once
#include "common.h"
#include "format.h"
#include "queue.h"
#include "sink.h"
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace detail {
class ThreadQueue;
}

// ---------------------------------------------------------------------------
// Logger — 核心日志类
// ---------------------------------------------------------------------------
class Logger {
public:
  static Logger &instance();

  // 可选配置
  Logger &level(Level lvl);
  Level level() const;
  bool shouldLog(Level lvl) const;
  Logger &headerPattern(std::string pattern);
  Logger &addRotatingFileSink(std::string path, size_t maxSize = 10 * 1024 * 1024,
                              size_t maxBackups = 3);
  void stop() noexcept;

  void log(Level lvl, std::string message, SourceLocation src = {});

private:
  struct QueueConfig {
    size_t capacity = 64;
    uint32_t maxMessageSize = 512;
  };

  Logger() = default;
  ~Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  std::string formatLine(const LogEntry &entry);
  void writeLineToSinks(std::string_view line, Level lvl, bool flushAfterWrite);
  void workerLoop() noexcept;
  void drainQueues(bool drainAll);
  std::shared_ptr<detail::ThreadQueue> thisThreadQueueOwned();

  std::atomic<Level> minLevel_{Level::DEBUG};
  static thread_local std::shared_ptr<detail::ThreadQueue> t_queue_;

  std::vector<std::unique_ptr<Sink>> sinks_;
  std::atomic<bool> running_{false};
  mutable std::mutex stateMutex_;
  mutable std::mutex configMutex_;
  std::thread worker_;
  std::chrono::milliseconds pollInterval_{5};
  QueueConfig queueConfig_{};
  std::map<std::thread::id, std::shared_ptr<detail::ThreadQueue>> producerMap_;
  HeaderFormatter headerFmt_{"{HMSf} [{level}] "};
  bool autoInitialized_ = false;
};

// ---------------------------------------------------------------------------
// 全局便捷函数 — 类似 spdlog 的用法
// ---------------------------------------------------------------------------
namespace chlog {

template <typename... Args>
inline void debug(fmt::format_string<Args...> fmt, Args &&...args) {
  if (!Logger::instance().shouldLog(Level::DEBUG))
    return;
  Logger::instance().log(Level::DEBUG,
                        fmt::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void info(fmt::format_string<Args...> fmt, Args &&...args) {
  if (!Logger::instance().shouldLog(Level::INFO))
    return;
  Logger::instance().log(Level::INFO,
                        fmt::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void warn(fmt::format_string<Args...> fmt, Args &&...args) {
  if (!Logger::instance().shouldLog(Level::WARN))
    return;
  Logger::instance().log(Level::WARN,
                        fmt::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void error(fmt::format_string<Args...> fmt, Args &&...args) {
  if (!Logger::instance().shouldLog(Level::ERROR))
    return;
  Logger::instance().log(Level::ERROR,
                        fmt::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void critical(fmt::format_string<Args...> fmt, Args &&...args) {
  Logger::instance().log(Level::CRITICAL,
                        fmt::format(fmt, std::forward<Args>(args)...));
}

} // namespace chlog