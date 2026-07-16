#include "S.h"
#include "P.h"

int main(void) {
  if (spn_test_p_bump() != 1) return 1;
  if (spn_test_s() != 10) return 2;
  if (spn_test_p_bump() != 2) return 3;
  return 0;
}
