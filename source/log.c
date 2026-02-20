#include "log.h"

#include "ctx.h"

#include "sp/io.h"

spn_log_level_t spn_log_level_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "ERROR")) {
    return SPN_LOG_LEVEL_ERROR;
  }
  if (sp_str_equal_cstr(str, "WARN")) {
    return SPN_LOG_LEVEL_WARN;
  }
  if (sp_str_equal_cstr(str, "INFO")) {
    return SPN_LOG_LEVEL_INFO;
  }
  if (sp_str_equal_cstr(str, "DEBUG")) {
    return SPN_LOG_LEVEL_DEBUG;
  }

  SP_FATAL("Unknown SPN_LOG_LEVEL {:fg brightyellow}; options are [ERROR, WARN, INFO, DEBUG]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_LOG_LEVEL_INFO);
}

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
