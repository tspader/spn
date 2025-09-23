#define SP_IMPLEMENTATION
#include "sp.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H

int main() {
  FT_Library library;
  FT_Face face = SP_NULLPTR;
  FT_Error status;

  sp_init_default();
  if (FT_Init_FreeType(&library)) {
    SP_FATAL("Failed to initialize FreeType");
  }

  int major = 0;
  int minor = 0;
  int patch = 0;
  FT_Library_Version(library, &major, &minor, &patch);
  SP_LOG("FreeType, v{:fg brightred}.{:fg brightgreen}.{:fg brightcyan}", SP_FMT_S32(major), SP_FMT_S32(minor), SP_FMT_S32(patch));

  SP_LOG("{:fg brightcyan}", SP_FMT_STR(sp_os_get_executable_path()));
  status = FT_New_Face(library, "/usr/share/fonts/TTF/Hack-Regular.ttf", 0, &face);
  if (status) {
    SP_FATAL("Failed to load face: {:fg brightblack}", SP_FMT_S32(status));
  }
  FT_Set_Pixel_Sizes(face, 0, 24);


  sp_str_t text = sp_str_lit("hello");

  // Store character bitmap data
  unsigned char* bitmap_data[256];
  int char_widths[256];
  int char_heights[256];
  int max_height = 0;

  // Load all characters and copy bitmap data
  for (int i = 0; i < text.len; i++) {
    c8 ch = sp_str_at(text, i);
    if (ch == ' ') {
      bitmap_data[i] = SP_NULLPTR;
      char_widths[i] = 8;
      char_heights[i] = 0;
      continue;
    }

    FT_Load_Char(face, ch, FT_LOAD_RENDER);
    FT_Bitmap* bitmap = &face->glyph->bitmap;

    char_widths[i] = bitmap->width;
    char_heights[i] = bitmap->rows;

    // Copy bitmap data
    int data_size = bitmap->width * bitmap->rows;
    bitmap_data[i] = malloc(data_size);
    memcpy(bitmap_data[i], bitmap->buffer, data_size);

    if (bitmap->rows > max_height) {
      max_height = bitmap->rows;
    }
  }

  // Render line by line
  for (int y = 0; y < max_height; y++) {
    for (int i = 0; i < text.len; i++) {
      if (bitmap_data[i] == SP_NULLPTR) {
        // Space character
        for (int x = 0; x < char_widths[i]; x++) {
          printf(" ");
        }
      } else {
        for (int x = 0; x < char_widths[i]; x++) {
          if (y < char_heights[i]) {
            unsigned char pixel = bitmap_data[i][y * char_widths[i] + x];
            if (pixel > 128) printf("#");
            else if (pixel > 64) printf("*");
            else if (pixel > 32) printf(".");
            else printf(" ");
          } else {
            printf(" ");
          }
        }
      }
    }
    printf("\n");
  }

  return 0;
}
