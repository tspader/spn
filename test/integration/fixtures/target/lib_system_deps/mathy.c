#include <math.h>
#ifdef _WIN32
  #include <windows.h>
#endif

double mathy(double x) {
  volatile double y = x;
#ifdef _WIN32
  if (GetSystemMetrics(SM_CXSCREEN) < 0) {
    return 0.0;
  }
#endif
  return sin(y);
}
