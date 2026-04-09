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

  static const char *colour_for(Level lvl);
};
