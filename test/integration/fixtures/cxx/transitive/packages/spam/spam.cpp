#include "spam.h"

struct base {
  virtual ~base() {}
  virtual int value() = 0;
};

struct derived : base {
  int value() override { return 69; }
};

extern "C" int spam_value(void) {
  base* b = new derived();
  int v = b->value();
  delete b;
  return v;
}
