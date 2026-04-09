#include "chlog.h"

chlog &chlog::instance() {
  static chlog ins;
  return ins;
}
