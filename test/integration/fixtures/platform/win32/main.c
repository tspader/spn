#include <stdio.h>
#include <windows.h>

int main() {
  printf("%llu %d\n", (unsigned long long)GetTickCount64(), GetSystemMetrics(SM_CXSCREEN));
  return 0;
}
