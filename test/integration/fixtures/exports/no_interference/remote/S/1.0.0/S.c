#include "S.h"
#include "P.h"

int spn_test_s(void) {
  int value = 0;
  for (int it = 0; it < 10; it++) {
    value = spn_test_p_bump();
  }
  return value;
}
