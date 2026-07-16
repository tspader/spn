#include "P.h"

int spn_test_p_bump(void) {
  static int counter = 0;
  return ++counter;
}
