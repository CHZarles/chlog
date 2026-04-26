#include "sink.h"

#include <cerrno>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
std::string errno_message(const char *operation, const std::string &path) {
  const int err = errno;
  std::ostringstream out;
  out << "RotatingFileSink: " << operation << " '" << path
      << "': " << std::strerror(err);
  return out.str();
}

std::string errno_message(const char *operation, const std::string &src,
                          const std::string &dst) {
  const int err = errno;
  std::ostringstream out;
  out << "RotatingFileSink: " << operation << " '" << src << "' to '" << dst
      << "': " << std::strerror(err);
  return out.str();
}
} // namespace

void ConsoleSink::write(std::string_view message, Level level) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (useColour_) {
    std::cout << colour_for(level) << message << "\033[0m\n";
  } else {
    std::cout << message << "\n";
  }
}

void ConsoleSink::flush() { std::cout.flush(); }

RotatingFileSink::RotatingFileSink(std::string filePath, size_t maxSize,
                                   size_t maxBackups)
    : filePath_(std::move(filePath)), maxSize_(maxSize),
      maxBackups_(maxBackups) {
  file_ = std::fopen(filePath_.c_str(), "a");
  if (!file_) {
    throw std::runtime_error(errno_message("cannot open", filePath_));
  }
  if (std::fseek(file_, 0, SEEK_END) != 0) {
    const std::string error = errno_message("cannot seek", filePath_);
    std::fclose(file_);
    file_ = nullptr;
    throw std::runtime_error(error);
  }

  long pos = std::ftell(file_);
  if (pos < 0) {
    const std::string error = errno_message("cannot tell", filePath_);
    std::fclose(file_);
    file_ = nullptr;
    throw std::runtime_error(error);
  }

  // size_t 的宽度在所有主流平台都大于 long
  curSize_ = static_cast<size_t>(pos);
}

RotatingFileSink::~RotatingFileSink() {
  if (file_) {
    std::fclose(file_);
  }
}

void RotatingFileSink::flush() noexcept { (void)flushChecked(); }

bool RotatingFileSink::flushChecked() noexcept {
  try {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!file_) {
      return failLiteral("RotatingFileSink: log file is not open");
    }
    if (std::fflush(file_) != 0) {
      return fail(errno_message("cannot flush", filePath_));
    }
    lastError_.clear();
    return true;
  } catch (...) {
    return failLiteral("RotatingFileSink: unexpected flush failure");
  }
}

bool RotatingFileSink::rotate() {
  if (!file_) {
    return failLiteral("RotatingFileSink: log file is not open");
  }

  if (std::fflush(file_) != 0) {
    return fail(errno_message("cannot flush", filePath_));
  }

  if (maxBackups_ == 0) {
    if (std::fclose(file_) != 0) {
      file_ = nullptr;
      return fail(errno_message("cannot close", filePath_));
    }
    file_ = nullptr;

    if (std::remove(filePath_.c_str()) != 0 && errno != ENOENT) {
      return fail(errno_message("cannot remove", filePath_));
    }

    file_ = std::fopen(filePath_.c_str(), "w");
    if (!file_) {
      return fail(errno_message("cannot open", filePath_));
    }
    curSize_ = 0;
    lastError_.clear();
    return true;
  }

  // 删掉后缀数字最大的文件
  std::ostringstream oldest;
  oldest << filePath_ << '.' << maxBackups_;
  if (std::remove(oldest.str().c_str()) != 0 && errno != ENOENT) {
    return fail(errno_message("cannot remove", oldest.str()));
  }

  // 往后挪动日志
  for (size_t n = maxBackups_; n > 1; --n) {
    std::ostringstream src, dst;
    src << filePath_ << '.' << (n - 1);
    dst << filePath_ << '.' << n;
    if (std::rename(src.str().c_str(), dst.str().c_str()) != 0 &&
        errno != ENOENT) {
      return fail(errno_message("cannot rename", src.str(), dst.str()));
    }
  }

  if (std::fclose(file_) != 0) {
    file_ = nullptr;
    return fail(errno_message("cannot close", filePath_));
  }
  file_ = nullptr;

  std::ostringstream backup;
  backup << filePath_ << ".1";
  if (std::rename(filePath_.c_str(), backup.str().c_str()) != 0 &&
      errno != ENOENT) {
    const std::string error = errno_message("cannot rename", filePath_,
                                            backup.str());
    file_ = std::fopen(filePath_.c_str(), "a");
    if (file_) {
      std::fseek(file_, 0, SEEK_END);
      const long pos = std::ftell(file_);
      curSize_ = pos < 0 ? 0 : static_cast<size_t>(pos);
    }
    return fail(error);
  }

  // 打开新文件, 重置 curSize_
  file_ = std::fopen(filePath_.c_str(), "w");
  if (!file_) {
    return fail(errno_message("cannot open", filePath_));
  }
  curSize_ = 0;
  lastError_.clear();
  return true;
}

void RotatingFileSink::write(std::string_view message, Level level) noexcept {
  (void)writeChecked(message, level);
}

bool RotatingFileSink::writeChecked(std::string_view message,
                                    Level /*level*/) noexcept {
  try {
    const size_t needed = message.size() + 1; // +1 for '\n'

    std::lock_guard<std::mutex> lock(mtx_);

    if (file_ && curSize_ + needed > maxSize_) {
      if (!rotate()) {
        return false;
      }
    }

    if (!file_) {
      return failLiteral("RotatingFileSink: log file is not open");
    }

    const size_t written =
        std::fwrite(message.data(), 1, message.size(), file_);
    if (written != message.size() || std::fputc('\n', file_) == EOF) {
      return fail(errno_message("cannot write", filePath_));
    }

    curSize_ += needed;
    lastError_.clear();
    return true;
  } catch (...) {
    return failLiteral("RotatingFileSink: unexpected write failure");
  }
}

std::string RotatingFileSink::lastError() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return lastError_;
}

bool RotatingFileSink::fail(std::string message) noexcept {
  try {
    lastError_ = std::move(message);
  } catch (...) {
    return failLiteral("RotatingFileSink: failed to record error");
  }
  return false;
}

bool RotatingFileSink::failLiteral(const char *message) noexcept {
  try {
    lastError_ = message;
  } catch (...) {
    lastError_.clear();
  }
  return false;
}
