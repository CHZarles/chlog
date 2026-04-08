#pragma once
#include <cstdint>

enum class Level : uint32_t {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
  CRITICAL = 4
};
