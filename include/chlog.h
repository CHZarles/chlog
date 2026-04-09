#pragma once
#include "common.h"
#include "format.h"

class chlog {
public:
  chlog &instance();

private:
  chlog() = default;
  chlog(const chlog &) = delete;
  chlog &operator=(const chlog &) = delete;
};
