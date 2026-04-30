#pragma once
#include "common.h"
#include "format.h"
#include "queue.h"
#include "sink.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace detail {
class ThreadQueue;
}

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

  std::string formatLine(const LogEntry &entry);
  bool tryEnqueue(const LogEntry &entry, uint64_t timestampUs);
  void writeLineToSinks(std::string_view line, Level lvl, bool flushAfterWrite);
  void workerLoop() noexcept;
  void drainQueues(bool drainAll);
  std::shared_ptr<detail::ThreadQueue> thisThreadQueueOwned();

  std::atomic<Level> minLevel_{Level::DEBUG};
  std::vector<std::unique_ptr<Sink>> sinks_;
  std::atomic<bool> running_{false};
  mutable std::mutex stateMutex_;
  std::condition_variable activeProducerCv_;
  mutable std::mutex configMutex_;
  std::thread worker_;
  std::chrono::milliseconds pollInterval_{5};
  QueueConfig queueConfig_{};
  size_t activeProducers_ = 0;
  std::map<std::thread::id, std::shared_ptr<detail::ThreadQueue>> producerMap_;
  HeaderFormatter headerFmt_; // 用默认配置
};
