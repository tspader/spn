#include <stdio.h>

#include "calc.h"

static int failures = 0;

#define expect(expr) \
  do { \
    if (!(expr)) { \
      printf("FAIL %s\n", #expr); \
      failures++; \
    } \
  } while (0)

int main(void) {
  expect(add(2, 3) == 5);
  expect(add(-1, 1) == 0);
  expect(multiply(2, 3) == 6);
  expect(multiply(7, 0) == 0);
  return failures;
}
