#include "pipeline.h"

int pipeline_value(void) {
  int v = 0;
#ifdef B_ON
  v += 1;
#endif
#ifdef C_ON
  v += 2;
#endif
  return v;
}
