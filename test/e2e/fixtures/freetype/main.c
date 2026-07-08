#include <ft2build.h>
#include FT_FREETYPE_H

int main() {
  FT_Library library;
  if (FT_Init_FreeType(&library)) return 1;
  FT_Done_FreeType(library);
  return 0;
}
