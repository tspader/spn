#ifdef WITH_TRACE
#include "tracer.h"
#endif

int main() {
#ifdef WITH_TRACE
  return tracer_value();
#else
  return 10;
#endif
}
