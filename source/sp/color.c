#include "color.h"

sp_str_t sp_color_to_tui_rgb(sp_mem_t mem, sp_color_t color) {
  return sp_color_to_tui_rgb_f(mem, color.r * 255, color.g * 255, color.b * 255);
}

sp_str_t sp_color_to_tui_rgb_f(sp_mem_t mem, u8 r, u8 g, u8 b) {
  return sp_fmt(mem, "\033[38;2;{};{};{}m", sp_fmt_uint(r), sp_fmt_uint(g), sp_fmt_uint(b)).value;
}
