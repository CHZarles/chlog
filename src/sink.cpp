#include "sink.h"

#include <cstring>
#include <sstream>
#include <stdexcept>

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

RotatingFileSink::RotatingFileSink(std::string filePath, size_t maxSize,
                                   size_t maxBackups)
    : filePath_(std::move(filePath)), maxSize_(maxSize),
      maxBackups_(maxBackups) {
  file_ = std::fopen(filePath_.c_str(), "a");
  if (!file_) {
    std::ostringstream err; // create a string buffer
    err << "RotatingFileSink: cannot open '" << filePath_
        << "' for append: " << std::strerror(errno);
    throw std::runtime_error(err.str());
  }
  if (std::fseek(file_, 0, SEEK_END) != 0) {
    throw std::runtime_error("fseek failed");
  }

  long pos = std::ftell(file_);
  if (pos < 0) {
    throw std::runtime_error("ftell failed");
  }

  // 8 byte : long and size_t
  curSize_ = static_cast<size_t>(pos);
}

RotatingFileSink::~RotatingFileSink() noexcept {
  if (file_) {
    std::fclose(file_);
  }
}
