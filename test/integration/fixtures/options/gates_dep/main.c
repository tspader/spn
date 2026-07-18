#ifdef NDEBUG
#ifndef WITH_TRACE
#error "expected WITH_TRACE"
#endif
#include "tracer.h"
#else
#ifdef WITH_TRACE
#error "unexpected WITH_TRACE"
#endif
#endif

int main() {
#ifdef NDEBUG
  return tracer_value();
#else
  return 10;
#endif
}
