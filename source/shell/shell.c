#include "shell/shell.h"

#include "ctx/types.h"
#include "event/flush.h"
#include "event/log.h"
#include "sp/io.h"
#include "tui/tui.h"

#ifdef SP_WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif

  #ifndef NOMINMAX
    #define NOMINMAX
  #endif

  #include <windows.h>
#endif

void spn_shell_init(spn_shell_t* shell) {
  shell->mem = sp_mem_os_new();

  sp_io_stream_writer_from_fd(&shell->logger.out, sp_sys_stdout, SP_IO_CLOSE_MODE_NONE);
  sp_io_stream_writer_from_fd(&shell->logger.err, sp_sys_stderr, SP_IO_CLOSE_MODE_NONE);
  if (sp_sys_is_tty(sp_sys_stdout)) {
    sp_sys_tty_use_vt(sp_sys_stdout);
  }
  if (sp_sys_is_tty(sp_sys_stderr)) {
    sp_sys_tty_use_vt(sp_sys_stderr);
  }
#ifdef SP_WIN32
  if (sp_sys_is_tty(sp_sys_stdout) || sp_sys_is_tty(sp_sys_stderr)) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
  }
#endif
}

void spn_shell_open(spn_shell_t* shell) {
  if (shell->cli.quiet) {
    shell->logger.verbosity = SPN_VERBOSITY_QUIET;
  } else if (shell->cli.verbose) {
    shell->logger.verbosity = SPN_VERBOSITY_VERBOSE;
  } else {
    shell->logger.verbosity = SPN_VERBOSITY_NORMAL;
  }

  spn_tui_mode_t mode = SPN_OUTPUT_MODE_INTERACTIVE;
  if (!sp_str_empty(shell->cli.output)) {
    mode = spn_output_mode_from_str(shell->cli.output);
  }
  spn_tui_init(&shell->tui, shell->mem, mode, &shell->logger);
}

void spn_shell_open_log(spn_shell_t* shell, sp_str_t dir) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t path = sp_fs_join_path(scratch.mem, dir, sp_str_lit("build.jsonl"));
  sp_fs_create_file(path);
  sp_io_file_writer_from_path(&shell->logger.jsonl, path);
  sp_mem_end_scratch(scratch);
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

static u32 short_thread_id(u64 thread_id) {
  static sp_ht(u64, u32) thread_map = SP_NULLPTR;
  static u32 id = 0;

  if (!thread_map) sp_ht_init(shell.mem, thread_map);
  if (!sp_ht_key_exists(thread_map, thread_id)) {
    sp_ht_insert(thread_map, thread_id, id++);
  }
  return *sp_ht_getp(thread_map, thread_id);
}

void spn_shell_flush() {
  if (!spn.events) return;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_da(spn_build_event_t) events = spn_event_flush(s.mem, spn.events);

  sp_da_for(events, it) {
    spn_build_event_t* event = &events[it];
    event->thread_id = short_thread_id(event->thread_id);

    if (shell.logger.jsonl.fd) {
      spn_event_log_jsonl(&shell.logger.jsonl.base, event);
    }

    spn_tui_log_event(&shell.tui, event);
  }

  sp_mem_end_scratch(s);
}
