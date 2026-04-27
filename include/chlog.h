#pragma once
#include "common.h"
#include "format.h"
#include "queue.h"
#include "sink.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class chlog {
public:
  static chlog &instance();

  chlog &level(Level lvl);

  Level level() const;

  bool shouldLog(Level lvl) const;

  void log(Level lvl, std::string message, SourceLocation src = {});

  chlog &addConsoleSink(bool useColour = true);
  chlog &addRotatingFileSink(std::string path, size_t maxSize,
                             size_t maxBackups = 3);
  chlog &queueConfig(size_t capacity, uint32_t maxMessageSize);
  chlog &start(std::chrono::milliseconds pollInterval =
                   std::chrono::milliseconds(5));
  void stop() noexcept;

private:
  struct QueueConfig {
    size_t capacity = 64;
    uint32_t maxMessageSize = 512;
  };

  chlog() = default;
  ~chlog();
  chlog(const chlog &) = delete;
  chlog &operator=(const chlog &) = delete;

  std::string formatLine(Level lvl, const std::string &message,
                         const std::string &sourceText);
  void writeLineToSinks(std::string_view line, Level lvl, bool flushAfterWrite);
  void workerLoop() noexcept;
  void drainQueue();

  std::atomic<Level> minLevel_{Level::DEBUG};
  std::vector<std::unique_ptr<Sink>> sinks_;
  std::atomic<bool> running_{false};
  mutable std::mutex stateMutex_;
  mutable std::mutex configMutex_;
  std::unique_ptr<RuntimeSPSCQueue> queue_;
  std::thread worker_;
  std::chrono::milliseconds pollInterval_{5};
  QueueConfig queueConfig_{};
  HeaderFormatter headerFmt_; // 用默认配置
};
