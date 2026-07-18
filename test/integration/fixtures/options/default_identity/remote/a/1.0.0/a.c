#include "a.h"

#if defined(A_X) && defined(A_Y)
int a_on() {
  return 3;
}
#elif !defined(A_X) && !defined(A_Y)
int a_off() {
  return 0;
}
#else
#error "unexpected default option state"
#endif
