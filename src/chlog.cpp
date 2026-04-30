#include "chlog.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace {
bool is_power_of_two(size_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

uint64_t timestamp_us_of(
    const std::chrono::system_clock::time_point &timestamp) {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          timestamp.time_since_epoch())
          .count());
}
} // namespace

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

std::string chlog::formatLine(const LogEntry &entry) {
  HeaderFormatter hdrFmt;
  {
    std::lock_guard<std::mutex> lock(configMutex_);
    hdrFmt = headerFmt_;
  }
  return hdrFmt.format(entry) + entry.message;
}

bool chlog::tryEnqueue(const LogEntry &entry) {
  const uint8_t nameLen = static_cast<uint8_t>(
      std::min(entry.threadName.size(), static_cast<size_t>(QueueMsg::NAME_CAPACITY)));
  const uint32_t messageLen = static_cast<uint32_t>(entry.message.size());

  std::lock_guard<std::mutex> lock(stateMutex_);
  if (!running_.load(std::memory_order_acquire) || !queue_) {
    return false;
  }

  const uint32_t maxPayloadBytes = queueConfig_.maxMessageSize;
  const uint32_t basePayloadBytes =
      QueueMsg::payload_bytes(nameLen, 0, messageLen);
  if (basePayloadBytes > maxPayloadBytes) {
    return false;
  }

  const uint16_t fileLen = static_cast<uint16_t>(std::min(
      entry.file.size(), static_cast<size_t>(maxPayloadBytes - basePayloadBytes)));
  const uint32_t packedSize = 3u + nameLen + fileLen + messageLen;
  const uint32_t payloadBytes =
      QueueMsg::payload_bytes(nameLen, fileLen, messageLen);

  MsgHeader *hdr = queue_->alloc(payloadBytes);
  if (!hdr) {
    return false;
  }

  char *const payload = hdr->payload();
  QueueMsg::size_ref(payload) = packedSize;
  QueueMsg::nameLen_ref(payload) = nameLen;
  if (nameLen > 0) {
    std::memcpy(payload + QueueMsg::NAME_OFFSET, entry.threadName.data(),
                nameLen);
  }
  QueueMsg::set_file_len(payload, nameLen, fileLen);
  if (fileLen > 0) {
    std::memcpy(payload + QueueMsg::file_offset(nameLen), entry.file.data(),
                fileLen);
  }
  std::memcpy(payload + QueueMsg::message_offset(nameLen, fileLen),
              entry.message.data(), messageLen);

  hdr->logId = static_cast<uint32_t>(entry.level);
  hdr->timestamp_us = timestamp_us_of(entry.timestamp);
  queue_->push();
  return true;
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

  if (!running_.load(std::memory_order_acquire)) {
    LogEntry entry(lvl, std::move(message), sourceText);
    const std::string line = formatLine(entry);
    writeLineToSinks(line, entry.level, true);
    return;
  }

  LogEntry entry(timestamp_us_of(std::chrono::system_clock::now()), lvl,
                 this_thread_name(), std::move(message), sourceText);
  if (!tryEnqueue(entry)) {
    const std::string line = formatLine(entry);
    writeLineToSinks(line, entry.level, true);
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
  if (!is_power_of_two(capacity)) {
    throw std::invalid_argument(
        "queue capacity must be a non-zero power of two");
  }
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
    uint64_t timestampUs = 0;
    Level lvl = Level::INFO;
    std::string threadName;
    std::string message;
    std::string sourceText;
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      if (!queue_)
        return;
      MsgHeader *hdr = queue_->front();
      if (!hdr)
        return;

      char *payload = hdr->payload();
      timestampUs = hdr->timestamp_us;
      lvl = static_cast<Level>(hdr->logId);
      threadName.assign(QueueMsg::threadName(payload));
      message.assign(QueueMsg::message(payload));
      sourceText.assign(QueueMsg::file(payload));
      queue_->pop();
    }
    LogEntry entry(timestampUs, lvl, std::move(threadName), std::move(message),
                   std::move(sourceText));
    const std::string line = formatLine(entry);
    writeLineToSinks(line, entry.level, false);
  }
}
