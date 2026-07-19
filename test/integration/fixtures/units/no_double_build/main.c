#include "foo.h"

#if FOO_VERSION != 15
#error "expected foo 1.5.0"
#endif

int main() {
  return foo_version() == FOO_VERSION ? 0 : 1;
}
