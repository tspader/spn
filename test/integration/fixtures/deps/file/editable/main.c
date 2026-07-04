#include "spum.h"

int main(int num_args, const char** args) {
  (void)num_args;
  (void)args;

#if defined(SPUM)
  return 69;
#elif defined(KRAM)
  return 42;
#else
  #error "expected SPUM or KRAM"
#endif
}
