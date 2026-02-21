#include "color.h"

sp_str_t sp_color_to_tui_rgb(sp_color_t color) {
  return sp_color_to_tui_rgb_f(color.r * 255, color.g * 255, color.b * 255);
}

sp_str_t sp_color_to_tui_rgb_f(u8 r, u8 g, u8 b) {
  return sp_format("\033[38;2;{};{};{}m", SP_FMT_U32(r), SP_FMT_U32(g), SP_FMT_U32(b));
}
