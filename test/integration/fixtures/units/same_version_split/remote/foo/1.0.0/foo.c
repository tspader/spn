#include "foo.h"
#include "num.h"

int foo_value(void) {
  return num_version() + 100;
}
