#pragma once
#include "common.h"
#include "format.h"
#include "sink.h"
#include <atomic>
#include <mutex>
#include <memory>
#include <vector>

class chlog {
public:
  static chlog &instance();

  chlog &level(Level lvl);

  Level level() const;

  bool shouldLog(Level lvl) const;

  void log(Level lvl, std::string message, SourceLocation src = {});

  chlog &addConsoleSink(bool useColour = true);

private:
  chlog() = default;
  chlog(const chlog &) = delete;
  chlog &operator=(const chlog &) = delete;

  std::atomic<Level> minLevel_{Level::DEBUG};
  std::vector<std::unique_ptr<Sink>> sinks_;
  std::atomic<bool> running_{false};
  mutable std::mutex configMutex_;
  HeaderFormatter headerFmt_; // 用默认配置
};
