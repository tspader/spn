#define SP_IMPLEMENTATION
#include "sp.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#define match(v) switch ((size_t)(v))

s32 main() {
  FT_Library library;
  FT_Face face = SP_NULLPTR;
  FT_Error status;

  sp_init_default();
  if (FT_Init_FreeType(&library)) {
    SP_FATAL("Failed to initialize FreeType");
  }

  s32 major = 0;
  s32 minor = 0;
  s32 patch = 0;
  FT_Library_Version(library, &major, &minor, &patch);
  SP_LOG("FreeType, v{:fg brightred}.{:fg brightgreen}.{:fg brightcyan}", SP_FMT_S32(major), SP_FMT_S32(minor), SP_FMT_S32(patch));

  sp_str_t executable = sp_os_get_executable_path(); // i.e. /build/examples/freetype/main

  sp_str_t project = executable;
  for (u32 i = 0; i < 3; i++) {
    project = sp_os_parent_path(project);
  }

  sp_str_t asset = sp_os_join_path(project, sp_str_lit("asset"));
  sp_str_t fonts = sp_os_join_path(asset, sp_str_lit("fonts"));

  sp_str_t font = sp_os_join_path(fonts, sp_str_lit("Hack-Regular.ttf"));
  u32 font_size = 20;
  sp_str_t text = sp_str_lit("hello, world!");

  SP_LOG(
    "Using font {:fg brightcyan} to print {:fg brightyellow}",
    SP_FMT_STR(font),
    SP_FMT_STR(text)
  );

  status = FT_New_Face(library, sp_str_to_cstr(font), 0, &face);
  if (status) {
    SP_FATAL("Failed to load face: {:fg brightblack}", SP_FMT_S32(status));
  }
  FT_Set_Pixel_Sizes(face, 0, font_size);

  typedef struct {
    u8* bitmap_data;
    s32 width;
    s32 height;
    s32 bitmap_top;
    s32 bitmap_left;
  } glyph_t;

  glyph_t glyphs[256];
  s32 max_ascender = 0;
  s32 max_descender = 0;

  // Load all characters and copy bitmap data
  for (u32 i = 0; i < text.len; i++) {
    c8 ch = sp_str_at(text, i);
    if (ch == ' ') {
      glyphs[i].bitmap_data = SP_NULLPTR;
      glyphs[i].width = 8;
      glyphs[i].height = 0;
      glyphs[i].bitmap_top = 0;
      glyphs[i].bitmap_left = 0;
      continue;
    }

    FT_Load_Char(face, ch, FT_LOAD_RENDER);
    FT_Bitmap* bitmap = &face->glyph->bitmap;

    glyphs[i].width = bitmap->width;
    glyphs[i].height = bitmap->rows;
    glyphs[i].bitmap_top = face->glyph->bitmap_top;
    glyphs[i].bitmap_left = face->glyph->bitmap_left;

    // Copy bitmap data
    s32 data_size = bitmap->width * bitmap->rows;
    glyphs[i].bitmap_data = malloc(data_size);
    memcpy(glyphs[i].bitmap_data, bitmap->buffer, data_size);

    // Track max ascender and descender for baseline alignment
    if (face->glyph->bitmap_top > max_ascender) {
      max_ascender = face->glyph->bitmap_top;
    }
    s32 descender = glyphs[i].height - face->glyph->bitmap_top;
    if (descender > max_descender) {
      max_descender = descender;
    }
  }

  s32 total_height = max_ascender + max_descender;

  // Render line by line with proper baseline alignment
  for (s32 y = 0; y < total_height; y++) {
    for (s32 i = 0; i < text.len; i++) {
      match(glyphs[i].bitmap_data) {
      }
      if (glyphs[i].bitmap_data == SP_NULLPTR) {
        // Space character
        for (s32 x = 0; x < glyphs[i].width; x++) {
          printf(" ");
        }
      }
      else {
        // Calculate the y position in the character's bitmap based on baseline
        s32 char_y = y - (max_ascender - glyphs[i].bitmap_top);

        for (s32 x = 0; x < glyphs[i].width; x++) {
          if (char_y >= 0 && char_y < glyphs[i].height) {
            unsigned char pixel = glyphs[i].bitmap_data[char_y * glyphs[i].width + x];

            // Convert to grayscale and use truecolor terminal escapes
            sp_str_t block = sp_format(
              "\033[38;2;{};{};{}m{}\033[0m",
              SP_FMT_U32(pixel),
              SP_FMT_U32(pixel),
              SP_FMT_U32(pixel),
              SP_FMT_CSTR(pixel > 32 ? "█" : " ")
            );
            printf("%.*s", block.len, block.data);
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
