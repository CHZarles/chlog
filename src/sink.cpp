#include "sink.h"

void ConsoleSink::write(std::string_view message, Level level) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (useColour_) {
    std::cout << colour_for(level) << message << "\033[0m\n";
  } else {
    std::cout << message << "\n";
  }
}

void ConsoleSink::flush() { std::cout.flush(); }

const char *ConsoleSink::colour_for(Level lvl) {
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
