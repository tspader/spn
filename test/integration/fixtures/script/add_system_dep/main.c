#include <math.h>
#ifdef _WIN32
  #include <windows.h>
#endif

int main(void) {
  volatile double x = sqrt(2.0);
  (void)x;
#ifdef _WIN32
  if (GetSystemMetrics(SM_CXSCREEN) < 0) {
    return 1;
  }
#endif
  return 0;
}
