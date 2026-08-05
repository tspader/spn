#include "sp.h"
#include "sp/macro.h"
#include "log/log.h"

#include "shell/types.h"

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

  SP_FATAL("Unknown SPN_LOG_LEVEL {.yellow}; options are [ERROR, WARN, INFO, DEBUG]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_LOG_LEVEL_INFO);
}

sp_str_t spn_log_level_to_str(spn_log_level_t level) {
  switch (level) {
    case SPN_LOG_LEVEL_ERROR: return sp_str_lit("error");
    case SPN_LOG_LEVEL_WARN:  return sp_str_lit("warn ");
    case SPN_LOG_LEVEL_INFO:  return sp_str_lit("info ");
    case SPN_LOG_LEVEL_DEBUG: return sp_str_lit("debug");
  }
  return sp_str_lit("Unknown log level");
}


static void write_line(sp_io_writer_t* io, const c8* fmt, va_list args) {
  sp_fmt_io_v(io, sp_cstr_as_str(fmt), args);
  sp_io_write_new_line(io);
}

void spn_print(const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  write_line(&shell.logger.out.base, fmt, args);
  va_end(args);
}

void spn_print_err(const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  write_line(&shell.logger.err.base, fmt, args);
  va_end(args);
}

void spn_log_info(const c8* fmt, ...) {
  if (shell.logger.level < SPN_LOG_LEVEL_INFO) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  write_line(&shell.logger.out.base, fmt, args);
  va_end(args);
}

void spn_log_warn(const c8* fmt, ...) {
  if (shell.logger.level < SPN_LOG_LEVEL_WARN) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  write_line(&shell.logger.err.base, fmt, args);
  va_end(args);
}

void spn_log_error(const c8* fmt, ...) {
  if (shell.logger.level < SPN_LOG_LEVEL_ERROR) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  write_line(&shell.logger.err.base, fmt, args);
  va_end(args);
}

void spn_log_debug(const c8* fmt, ...) {
  if (shell.logger.level < SPN_LOG_LEVEL_DEBUG) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  write_line(&shell.logger.err.base, fmt, args);
  va_end(args);
}
