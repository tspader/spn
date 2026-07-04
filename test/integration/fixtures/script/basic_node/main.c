#include "version.h"

#include <stdio.h>

int main(void) {
  printf("Version: %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

  _Static_assert(VERSION_MINOR == 2, "VERSION_MINOR should be 2");
  _Static_assert(VERSION_PATCH == 3, "VERSION_PATCH should be 3");

  return 0;
}
