#define STB_SPRINTF_IMPLEMENTATION
#include "foo_sprintf.h"
#include "stdio.h"

int main(void) {
  char buffer [256] = {0};
  stbsp_sprintf(buffer, "%s says: hello, %s!", __func__, "world");
  printf("stbsp_printf() returned formatted buffer: %s\n", buffer);

  return 0;
}
