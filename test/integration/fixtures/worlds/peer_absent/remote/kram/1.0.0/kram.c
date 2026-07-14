#include "kram.h"

#ifdef KRAM_UV
#include "spum.h"

int kram_uv(void) {
  return spum_value();
}
#else
int kram_uv(void) {
  return -1;
}
#endif
