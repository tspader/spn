#define _GNU_SOURCE
#include <dlfcn.h>
#include "S.h"

int main(void) {
  if (spn_test_s() != 1) return 1;
  if (!dlsym(RTLD_DEFAULT, "spn_test_s")) return 2;
  return 0;
}
