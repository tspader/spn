// This file will fail to compile if version.h is not generated
#include "version.h"

#include <stdio.h>

int main(void) {
  // Verify the generated constants exist
  printf("Version: %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
  
  // Static assertions to ensure values are correct
  _Static_assert(VERSION_MAJOR == 1, "VERSION_MAJOR should be 1");
  _Static_assert(VERSION_MINOR == 2, "VERSION_MINOR should be 2");
  _Static_assert(VERSION_PATCH == 3, "VERSION_PATCH should be 3");
  
  return 0;
}
