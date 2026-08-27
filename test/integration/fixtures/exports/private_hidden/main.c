#ifdef _WIN32
  #include <windows.h>
#else
  #define _GNU_SOURCE
  #include <dlfcn.h>
#endif
#include "S.h"

static void* find_symbol(const char* name) {
#ifdef _WIN32
  return (void*)GetProcAddress(GetModuleHandleA("S"), name);
#else
  return dlsym(RTLD_DEFAULT, name);
#endif
}

int main(void) {
  if (spn_test_s() != 1) return 1;
  if (find_symbol("spn_test_p")) return 2;
  return 0;
}
