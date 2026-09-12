#include "spum.h"
#include <math.h>
#ifdef _WIN32
  #include <windows.h>
#endif

int spum_value(int n) {
#ifdef _WIN32
  if (GetSystemMetrics(SM_CXSCREEN) < 0) {
    return 0;
  }
#endif
  return (int)sqrt((double)n);
}
