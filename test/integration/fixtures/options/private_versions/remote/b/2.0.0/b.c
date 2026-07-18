#include "b.h"

#ifdef B_X
int b_two_on() {
  return 4;
}
#else
int b_two_off() {
  return 3;
}
#endif
