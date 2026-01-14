#include <math.h>

int main(void) {
  volatile double x = sqrt(2.0);
  (void)x;
  return 0;
}
