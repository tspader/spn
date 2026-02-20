#include "final.h"
#include <stdio.h>

int main(void) {
  printf("Base: %d\n", BASE_VAL);
  printf("Left: %d\n", LEFT_VAL);
  printf("Right: %d\n", RIGHT_VAL);
  printf("Final: %d\n", FINAL_VAL);

  _Static_assert(BASE_VAL == 10, "BASE_VAL should be 10");
  _Static_assert(LEFT_VAL == 20, "LEFT_VAL should be 20");
  _Static_assert(RIGHT_VAL == 30, "RIGHT_VAL should be 30");
  _Static_assert(FINAL_VAL == 50, "FINAL_VAL should be 50");

  return 0;
}
