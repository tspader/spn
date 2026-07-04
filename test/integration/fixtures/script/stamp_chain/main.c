#include "result.h"
#include <stdio.h>

int main(void) {
  printf("Chain complete: %d\n", CHAIN_COMPLETE);
  _Static_assert(CHAIN_COMPLETE == 1, "CHAIN_COMPLETE should be 1");
  return 0;
}
