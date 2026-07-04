#include "config.h"
#include <stdio.h>

int main(void) {
  printf("Base: %d, Mult: %d, Result: %d\n", MY_BASE, MY_MULT, MY_RESULT);

  _Static_assert(MY_BASE == 7, "MY_BASE should be 7");
  _Static_assert(MY_MULT == 6, "MY_MULT should be 6");
  _Static_assert(MY_RESULT == 42, "MY_RESULT should be 42");

  return 0;
}
