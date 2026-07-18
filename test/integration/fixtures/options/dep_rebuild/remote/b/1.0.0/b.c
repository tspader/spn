#include "b.h"

#ifdef A_X
int b_on() {
  return 2;
}
#else
int b_off() {
  return 1;
}
#endif
