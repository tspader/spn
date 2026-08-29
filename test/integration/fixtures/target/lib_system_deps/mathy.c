#include <math.h>

double mathy(double x) {
  volatile double y = x;
  return sin(y);
}
