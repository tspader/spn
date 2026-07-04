#include "spum.h"

#if !defined(SPUM)
  #error "Expected header from the remote source repo to define SPUM, but it didn't"
#endif

int main() {
  return spum_answer() == 42 ? 0 : 1;
}
