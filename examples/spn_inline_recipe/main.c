#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

int main(void) {
  stbsp_sprintf("%s says: hello, %s!", __func__, "world");

  return 0;
}
