#include "spum.h"

#if !defined(KRAM)
  #error "expected the edited header to define KRAM"
#endif

#if defined(SPUM)
  #error "expected the edited header to replace SPUM"
#endif

int main() {
  return 0;
}
