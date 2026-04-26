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

  // size_t 的宽度在所有主流平台都大于 long
  curSize_ = static_cast<size_t>(pos);
}

RotatingFileSink::~RotatingFileSink() {
  if (file_) {
    std::fclose(file_);
  }
}

void RotatingFileSink::flush() {
  std::lock_guard<std::mutex> lock(mtx_);
  if (file_)
    std::fflush(file_);
}

void RotatingFileSink::rotate() {
  if (file_) {
    std::fclose(file_);
    file_ = nullptr;
  }

  if (maxBackups_ == 0) {
    std::remove(filePath_.c_str());
    file_ = std::fopen(filePath_.c_str(), "w");
    curSize_ = 0;
    return;
  }

  // 删掉后缀数字最大的文件
  std::ostringstream oldest;
  oldest << filePath_ << '.' << maxBackups_;
  std::remove(oldest.str().c_str());

  // 往后挪动日志
  for (size_t n = maxBackups_; n > 1; --n) {
    std::ostringstream src, dst;
    src << filePath_ << '.' << (n - 1);
    dst << filePath_ << '.' << n;
    std::rename(src.str().c_str(), dst.str().c_str());
  }

  std::ostringstream backup;
  backup << filePath_ << ".1";
  std::rename(filePath_.c_str(), backup.str().c_str());

  // 打开新文件, 重置 curSize_
  file_ = std::fopen(filePath_.c_str(), "w");
  curSize_ = 0;
}

void RotatingFileSink::write(std::string_view message, Level /*level*/) {
  const size_t needed = message.size() + 1; // +1 for '\n'

  std::lock_guard<std::mutex> lock(mtx_);

  if (file_ && curSize_ + needed > maxSize_) {
    rotate();
  }

  if (!file_) { // 新建文件失败，直接返回
    return;
  }

  const size_t written = std::fwrite(message.data(), 1, message.size(), file_);
  if (written != message.size() ||
      std::fputc('\n', file_) == EOF) { // 失败直接返回
    return;
  }

  curSize_ += needed;
}
