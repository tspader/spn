#include "foo.h"
#include "gfx.h"

int main() {
  if (FOO_VERSION != 20) return 1;
  if (foo_version() != 20) return 2;
  gfx_foo_version();
  return 0;
}
