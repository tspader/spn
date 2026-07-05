#include "foo.h"

int main() {
  if (FOO_VERSION != 20) return 1;
  if (foo_version() != 20) return 2;
  return 0;
}
