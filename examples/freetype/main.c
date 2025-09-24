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
  sp_str_t font_path = sp_os_join_path(sp_os_get_executable_path(), sp_str_view("Hack-Regular.ttf"));
  status = FT_New_Face(library, sp_str_to_cstr(font_path), 0, &face);
  if (status) {
    SP_FATAL("Failed to load face: {:fg brightblack}", SP_FMT_S32(status));
  }
  FT_Set_Pixel_Sizes(face, 0, 16);


  sp_str_t text = sp_str_lit("hello, world!");

  // Store character bitmap data and metrics
  unsigned char* bitmap_data[256];
  int char_widths[256];
  int char_heights[256];
  int char_bitmap_top[256];
  int char_bitmap_left[256];
  int max_ascender = 0;
  int max_descender = 0;

  // Load all characters and copy bitmap data
  for (int i = 0; i < text.len; i++) {
    c8 ch = sp_str_at(text, i);
    if (ch == ' ') {
      bitmap_data[i] = SP_NULLPTR;
      char_widths[i] = 8;
      char_heights[i] = 0;
      char_bitmap_top[i] = 0;
      char_bitmap_left[i] = 0;
      continue;
    }

    FT_Load_Char(face, ch, FT_LOAD_RENDER);
    FT_Bitmap* bitmap = &face->glyph->bitmap;

    char_widths[i] = bitmap->width;
    char_heights[i] = bitmap->rows;
    char_bitmap_top[i] = face->glyph->bitmap_top;
    char_bitmap_left[i] = face->glyph->bitmap_left;

    // Copy bitmap data
    int data_size = bitmap->width * bitmap->rows;
    bitmap_data[i] = malloc(data_size);
    memcpy(bitmap_data[i], bitmap->buffer, data_size);

    // Track max ascender and descender for baseline alignment
    if (face->glyph->bitmap_top > max_ascender) {
      max_ascender = face->glyph->bitmap_top;
    }
    int descender = char_heights[i] - face->glyph->bitmap_top;
    if (descender > max_descender) {
      max_descender = descender;
    }
  }

  int total_height = max_ascender + max_descender;

  // Render line by line with proper baseline alignment
  for (int y = 0; y < total_height; y++) {
    for (int i = 0; i < text.len; i++) {
      if (bitmap_data[i] == SP_NULLPTR) {
        // Space character
        for (int x = 0; x < char_widths[i]; x++) {
          printf(" ");
        }
      } else {
        // Calculate the y position in the character's bitmap based on baseline
        int char_y = y - (max_ascender - char_bitmap_top[i]);

        for (int x = 0; x < char_widths[i]; x++) {
          if (char_y >= 0 && char_y < char_heights[i]) {
            unsigned char pixel = bitmap_data[i][char_y * char_widths[i] + x];

            // Convert to grayscale and use truecolor terminal escapes
            int gray = pixel;
            printf("\033[38;2;%d;%d;%dm", gray, gray, gray);

            // Use Unicode block characters based on brightness
            if (pixel > 192) printf("█");      // Full block
            else if (pixel > 128) printf("▓"); // Dark shade
            else if (pixel > 64) printf("▒");  // Medium shade
            else if (pixel > 32) printf("░");  // Light shade
            else printf(" ");

            printf("\033[0m"); // Reset color
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
