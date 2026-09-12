#include <stdio.h>
#include <math.h>
#ifdef _WIN32
  #include <windows.h>
#endif

int main(int num_args, const char** args) {
  volatile double x = 69.0;
#ifdef _WIN32
  printf("screen is %d\n", GetSystemMetrics(SM_CXSCREEN));
#endif
  printf("sin is %f\n", sin(x));
  return 0;
}
