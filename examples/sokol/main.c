#include <stdio.h>
#include <stdint.h>

#define SOKOL_IMPL
#define SOKOL_TIME_IMPL
#include <sokol_time.h>

int main(void) {
  stm_setup();
  uint64_t start = stm_now();
  uint64_t end = stm_now();
  printf("elapsed: %.6f seconds\n", stm_sec(end - start));
  return 0;
}
