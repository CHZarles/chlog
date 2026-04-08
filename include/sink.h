#pragma once
#include "common.h"
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
