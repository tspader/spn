#include "all.h"
#include <stdio.h>

int main(void) {
  my_int_t x = 5;
  my_float_t f = 3.14f;
  
  printf("x = %d, f = %f\n", x, (double)f);
  printf("MAX_SIZE = %d, MIN_SIZE = %d\n", MAX_SIZE, MIN_SIZE);
  printf("SQUARE(5) = %d, CUBE(3) = %d\n", SQUARE(5), CUBE(3));

  _Static_assert(MAX_SIZE == 1024, "MAX_SIZE should be 1024");
  _Static_assert(MIN_SIZE == 16, "MIN_SIZE should be 16");
  _Static_assert(SQUARE(5) == 25, "SQUARE(5) should be 25");
  _Static_assert(CUBE(3) == 27, "CUBE(3) should be 27");

  return 0;
}
