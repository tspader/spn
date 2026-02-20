#include "str.h"

sp_str_t sp_str_pad_ex(sp_str_t str, u32 n, c8 c) {
  if (str.len >= n) {
    return sp_str_copy(str);
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&builder, str);
  for (u32 it = str.len; it < n; it++) {
    sp_str_builder_append_c8(&builder, c);
  }

  return sp_str_builder_to_str(&builder);
}

sp_str_t sp_str_repeat(c8 c, u32 len) {
  if (!len) {
    return SP_ZERO_STRUCT(sp_str_t);
  }

  c8* buffer = (c8*)sp_alloc(len);
  sp_mem_fill(buffer, len, &c, sizeof(c8));
  return sp_str(buffer, len);
}

sp_str_t sp_str_map_kernel_colorize(sp_str_map_context_t* context) {
  sp_str_t id = *(sp_str_t*)context->user_data;
  sp_str_t ansi = sp_format_color_id_to_ansi_fg(id);
  return sp_format("{}{}{}", SP_FMT_STR(ansi), SP_FMT_STR(context->str), SP_FMT_CSTR(SP_ANSI_RESET));
}
