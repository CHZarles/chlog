#include "chlog.h"

#include <cstring>
#include <stdexcept>
#include <thread>

chlog &chlog::instance() {
  static chlog ins;
  return ins;
}

chlog::~chlog() { stop(); }

chlog &chlog::level(Level lvl) {
  minLevel_.store(lvl, std::memory_order_relaxed);
  return *this;
}
Level chlog::level() const { return minLevel_.load(std::memory_order_relaxed); }

bool chlog::shouldLog(Level lvl) const {
  return lvl >= minLevel_.load(std::memory_order_relaxed);
}

std::string chlog::formatLine(Level lvl, const std::string &message,
                              const std::string &sourceText) {
  HeaderFormatter hdrFmt;
  {
    std::lock_guard<std::mutex> lock(configMutex_);
    hdrFmt = headerFmt_;
  }
  LogEntry entry(lvl, message, sourceText);
  return hdrFmt.format(entry) + message;
}

void chlog::writeLineToSinks(std::string_view line, Level lvl,
                             bool flushAfterWrite) {
  std::lock_guard<std::mutex> lock(configMutex_);
  for (auto &sink : sinks_)
    sink->write(line, lvl);
  if (flushAfterWrite) {
    for (auto &sink : sinks_)
      sink->flush();
  }
}

void chlog::log(Level lvl, std::string message, SourceLocation src) {
  if (!shouldLog(lvl))
    return;

  const std::string sourceText = format_source_location(src);
  const std::string line = formatLine(lvl, message, sourceText);

  if (!running_.load(std::memory_order_acquire)) {
    writeLineToSinks(line, lvl, true);
    return;
  }

  bool queued = false;
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (running_.load(std::memory_order_acquire) && queue_) {
      MsgHeader *hdr = queue_->alloc(static_cast<uint32_t>(line.size()));
      if (hdr != nullptr) {
        hdr->logId = static_cast<uint32_t>(lvl);
        std::memcpy(hdr->payload(), line.data(), line.size());
        queue_->push();
        queued = true;
      }
    }
  }

  if (!queued) {
    writeLineToSinks(line, lvl, true);
  }
}

chlog &chlog::addConsoleSink(bool useColour) {
  std::lock_guard<std::mutex> lock(configMutex_);
  sinks_.push_back(std::make_unique<ConsoleSink>(useColour));
  return *this;
}

chlog &chlog::addRotatingFileSink(std::string path, size_t maxSize,
                                  size_t maxBackups) {
  std::lock_guard<std::mutex> lock(configMutex_);
  sinks_.push_back(
      std::make_unique<RotatingFileSink>(std::move(path), maxSize, maxBackups));
  return *this;
}

chlog &chlog::queueConfig(size_t capacity, uint32_t maxMessageSize) {
  if (maxMessageSize == 0) {
    throw std::invalid_argument("queue max message size must be positive");
  }

  std::lock_guard<std::mutex> lock(stateMutex_);
  if (running_.load(std::memory_order_acquire)) {
    throw std::logic_error("queue configuration cannot change while running");
  }
  queueConfig_ = QueueConfig{capacity, maxMessageSize};
  return *this;
}

chlog &chlog::start(std::chrono::milliseconds pollInterval) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  if (running_.load(std::memory_order_acquire) || worker_.joinable())
    return *this;

  queue_ = std::make_unique<RuntimeSPSCQueue>(queueConfig_.capacity,
                                              queueConfig_.maxMessageSize);
  pollInterval_ = pollInterval;
  running_.store(true, std::memory_order_release);
  worker_ = std::thread(&chlog::workerLoop, this);
  return *this;
}

void chlog::stop() noexcept {
  std::thread worker;
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    running_.store(false, std::memory_order_release);
    if (worker_.joinable())
      worker = std::move(worker_);
  }

  if (worker.joinable())
    worker.join();

  std::lock_guard<std::mutex> lock(stateMutex_);
  queue_.reset();
}

void chlog::workerLoop() noexcept {
  while (running_.load(std::memory_order_acquire)) {
    drainQueue();
    if (!running_.load(std::memory_order_acquire))
      break;
    std::this_thread::sleep_for(pollInterval_);
  }

  drainQueue();
  std::lock_guard<std::mutex> lock(configMutex_);
  for (auto &sink : sinks_)
    sink->flush();
}

void chlog::drainQueue() {
  while (true) {
    std::string line;
    Level lvl = Level::INFO;
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      if (!queue_)
        return;
      MsgHeader *hdr = queue_->front();
      if (!hdr)
        return;

      line.assign(hdr->payload(), hdr->size);
      lvl = static_cast<Level>(hdr->logId);
      queue_->pop();
    }
    writeLineToSinks(line, lvl, false);
  }
}
