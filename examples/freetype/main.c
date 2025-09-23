#include <stdio.h>
#include <ft2build.h>
#include FT_FREETYPE_H

int main(void) {
  FT_Library library;
  if (FT_Init_FreeType(&library) != 0) {
    fprintf(stderr, "failed to initialize freetype\n");
    return 1;
  }
  int major = 0;
  int minor = 0;
  int patch = 0;
  FT_Library_Version(library, &major, &minor, &patch);
  printf("freetype version: %d.%d.%d\n", major, minor, patch);
  FT_Done_FreeType(library);
  return 0;
}
