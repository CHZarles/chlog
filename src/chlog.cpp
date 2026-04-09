#include "chlog.h"

chlog &chlog::instance() {
  static chlog ins;
  return ins;
}

chlog &chlog::level(Level lvl) {
  minLevel_.store(lvl, std::memory_order_relaxed);
  return *this;
}
Level chlog::level() const { return minLevel_.load(std::memory_order_relaxed); }

bool chlog::shouldLog(Level lvl) const {
  return lvl >= minLevel_.load(std::memory_order_relaxed);
}

void chlog::log(Level lvl, std::string message, SourceLocation src) {
  if (!shouldLog(lvl))
    return;

  const std::string sourceText = format_source_location(src);

  if (!running_.load(std::memory_order_acquire)) {
    HeaderFormatter hdrFmt;
    {
      std::lock_guard<std::mutex> lock(configMutex_);
      hdrFmt = headerFmt_;
      LogEntry entry(lvl, message, sourceText);
      std::string header = hdrFmt.format(entry);
      std::string line = header + message;
      for (auto &sink : sinks_)
        sink->write(line, lvl);
      for (auto &sink : sinks_)
        sink->flush();
    }
    return;
  }
}

chlog &chlog::addConsoleSink(bool useColour) {
  std::lock_guard<std::mutex> lock(configMutex_);
  sinks_.push_back(std::make_unique<ConsoleSink>(useColour));
  return *this;
}
