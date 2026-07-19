#include "foo.h"

#if FOO_VERSION != 19
#error "expected foo 1.9.0"
#endif

int main() {
  return foo_version() == FOO_VERSION ? 0 : 1;
}
