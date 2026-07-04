#include "spum.h"

struct base {
  virtual ~base() {}
  virtual int value() = 0;
};

struct derived : base {
  int value() override { return 69; }
};

extern "C" int spum_value(void) {
  base* b = new derived();
  derived* d = dynamic_cast<derived*>(b);

  int v = 0;
  try {
    throw d->value();
  } catch (int thrown) {
    v = thrown;
  }

  delete b;
  return v;
}
