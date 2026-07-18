#include "b.h"

#ifdef B_X
int b_one_on() {
  return 2;
}
#else
int b_one_off() {
  return 1;
}
#endif
