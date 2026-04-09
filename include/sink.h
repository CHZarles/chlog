#pragma once
#include "common.h"
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// Sink — 日志输出目标的抽象接口
// ---------------------------------------------------------------------------
class Sink {
public:
  virtual ~Sink() = default;
  Sink(const Sink &) = delete;
  Sink &operator=(const Sink &) = delete;

  virtual void write(std::string_view message, Level level) = 0;
  virtual void flush() = 0;

protected:
  Sink() = default;
};

// ---------------------------------------------------------------------------
// ConsoleSink — writes coloured output to stdout
// ---------------------------------------------------------------------------
class ConsoleSink : public Sink {
public:
  explicit ConsoleSink(bool useColour = true) : useColour_(useColour) {}

  void write(std::string_view message, Level level) override;
  void flush() override;

private:
  bool useColour_;
  std::mutex mtx_;

  const char *colour_for(Level lvl) {
    switch (lvl) {
    case Level::DEBUG:
      return "\033[36m"; // cyan
    case Level::INFO:
      return "\033[0m"; // reset
    case Level::WARN:
      return "\033[33m"; // yellow
    case Level::ERROR:
      return "\033[31m"; // red
    case Level::CRITICAL:
      return "\033[1;31m"; // bold red
    }
    return "\033[0m";
  }
};

// ---------------------------------------------------------------------------
// RotatingFileSink — writes to a plain text file and rotates on size limit
//
// Rotation is triggered when the next write would push the file past maxSize.
// Numbered backups (.1, .2, …) are shifted; extras are removed.
// ---------------------------------------------------------------------------
class RotatingFileSink : public Sink {
public:
  RotatingFileSink(std::string filePath, size_t maxSize = 10 * 1024 * 1024,
                   size_t maxBackups = 3);
  ~RotatingFileSink() override;

  void write(std::string_view message, Level level) override;
  void flush() override;

private:
  void rotate();

  const std::string filePath_;
  const size_t maxSize_;
  const size_t maxBackups_;

  std::mutex mtx_;
  std::FILE *file_ = nullptr;
  size_t curSize_ = 0;
};
