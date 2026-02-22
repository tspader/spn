#include "log.h"

#include "ctx.h"

#include "sp/io.h"

void spn_log_info(const c8* fmt, ...) {
  if (spn_ctx_get_log_level() < SPN_LOG_LEVEL_INFO) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_io_write_line(spn_ctx_get_log_out(), str);
}

void spn_log_warn(const c8* fmt, ...) {
  if (spn_ctx_get_log_level() < SPN_LOG_LEVEL_WARN) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_io_write_line(spn_ctx_get_log_err(), str);
}

void spn_log_error(const c8* fmt, ...) {
  if (spn_ctx_get_log_level() < SPN_LOG_LEVEL_ERROR) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_io_write_line(spn_ctx_get_log_err(), str);
}

void spn_log_debug(const c8* fmt, ...) {
  if (spn_ctx_get_log_level() < SPN_LOG_LEVEL_DEBUG) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_io_write_line(spn_ctx_get_log_err(), str);
}

void spn_ctx_tui(const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_io_write_line(spn_ctx_get_log_err(), str);
}
