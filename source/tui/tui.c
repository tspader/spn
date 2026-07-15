#include "profile/types.h"
#include "session/types.h"

#include "toml/issue.h"
#include "compiler/driver.h"
#include "ctx/ctx.h"
#include "enum/enum.h"
#include "event/event.h"
#include "log/log.h"
#include "semver/convert.h"
#include "sp/color.h"
#include "sp/io.h"
#include "sp/macro.h"
#include "sp/prompt.h"
#include "sp/str.h"
#include "spn.h"
#include "task/build/dag.h"
#include "toolchain/select.h"
#include "triple/triple.h"
#include "tui/tui.h"

#include <stdarg.h>
#include <stdio.h>

sp_app_result_t sp_prompt_app_on_poll(sp_app_t* app);
sp_app_result_t sp_prompt_app_on_init(sp_app_t* app);
void sp_prompt_app_on_deinit(sp_app_t* app);
sp_prompt_widget_t sp_prompt_progress_widget(sp_prompt_ctx_t* ctx, sp_prompt_progress_t config);

#ifdef SP_WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif

  #ifndef NOMINMAX
    #define NOMINMAX
  #endif

  #include <windows.h>
#endif

#if defined(SP_POSIX)
  #include <fcntl.h>
  #include <unistd.h>
#endif

#define SP_TUI_PRINT(command) sp_tui_print(sp_str_view(command))

static sp_str_t spn_tui_name_to_color(sp_mem_t mem, sp_str_t str);
static sp_str_t spn_tui_decorate_name(sp_mem_t mem, sp_str_t name, u32 padded_len, c8 pad);
static void     spn_tui_line_writer_flush(spn_tui_line_writer_t* writer);

spn_tui_mode_t spn_output_mode_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "interactive")) {
    return SPN_OUTPUT_MODE_INTERACTIVE;
  } else if (sp_str_equal_cstr(str, "noninteractive")) {
    return SPN_OUTPUT_MODE_NONINTERACTIVE;
  } else if (sp_str_equal_cstr(str, "quiet")) {
    return SPN_OUTPUT_MODE_QUIET;
  } else if (sp_str_equal_cstr(str, "none")) {
    return SPN_OUTPUT_MODE_NONE;
  }

  SP_FATAL("Unknown output mode {.yellow}; options are [interactive, noninteractive, quiet, none]", sp_fmt_str(str));
  SP_UNREACHABLE_RETURN(SPN_OUTPUT_MODE_NONE);
}

sp_str_t spn_output_mode_to_str(spn_tui_mode_t mode) {
  switch (mode) {
    SPN_OUTPUT_MODE(SP_X_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

void sp_tui_print(sp_str_t str) {
  sp_io_write_str(spn_ctx_get_log_err(), str, SP_NULLPTR);
}

void sp_tui_up(u32 n) {
  sp_fmt_io(spn_ctx_get_log_err(), "\033[{}A", sp_fmt_uint(n));
}

void sp_tui_down(u32 n) {
  sp_fmt_io(spn_ctx_get_log_err(), "\033[{}B", sp_fmt_uint(n));
}

void sp_tui_clear_line(void) {
  SP_TUI_PRINT("\033[K");
}

void sp_tui_show_cursor(void) {
  SP_TUI_PRINT("\033[?25h");
}

void sp_tui_hide_cursor(void) {
  SP_TUI_PRINT("\033[?25l");
}

void sp_tui_home(void) {
  SP_TUI_PRINT("\033[0G");
}

void sp_tui_flush(void) {
  fflush(stderr);
}

#ifdef SP_WIN32
void sp_tui_checkpoint(spn_tui_t* tui) {
  tui->terminal.input_handle = GetStdHandle(STD_INPUT_HANDLE);
  tui->terminal.output_handle = GetStdHandle(STD_OUTPUT_HANDLE);

  GetConsoleMode(tui->terminal.input_handle, (DWORD*)&tui->terminal.original_input_mode);
  GetConsoleMode(tui->terminal.output_handle, (DWORD*)&tui->terminal.original_output_mode);
  tui->terminal.modified = true;
}

void sp_tui_restore(spn_tui_t* tui) {
  if (tui->terminal.modified) {
    SetConsoleMode(tui->terminal.input_handle, (DWORD)tui->terminal.original_input_mode);
    SetConsoleMode(tui->terminal.output_handle, (DWORD)tui->terminal.original_output_mode);
  }
}

void sp_tui_setup_raw_mode(spn_tui_t* tui) {
  sp_win32_dword_t output_mode = tui->terminal.original_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(tui->terminal.output_handle, (DWORD)output_mode);

  sp_win32_dword_t input_mode = tui->terminal.original_input_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
  SetConsoleMode(tui->terminal.input_handle, (DWORD)input_mode);

  CONSOLE_CURSOR_INFO info;
  GetConsoleCursorInfo(tui->terminal.output_handle, &info);
  info.bVisible = FALSE;
  info.dwSize = 25;
  SetConsoleCursorInfo(tui->terminal.output_handle, &info);
}
#endif

#if defined(SP_POSIX)
void sp_tui_checkpoint(spn_tui_t* tui) {
  tcgetattr(STDIN_FILENO, &tui->terminal.ios);
  tui->terminal.modified = true;
}

void sp_tui_restore(spn_tui_t* tui) {
  if (tui->terminal.modified) {
    tcsetattr(STDIN_FILENO, TCSANOW, &tui->terminal.ios);
  }
}

void sp_tui_setup_raw_mode(spn_tui_t* tui) {
  struct termios ios = tui->terminal.ios;
  ios.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &ios);
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}
#endif

static sp_str_t spn_tui_decorate_name(sp_mem_t mem, sp_str_t name, u32 padded_len, c8 pad) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &w);
  sp_io_write_str(&w.base, spn_tui_name_to_color(mem, name), SP_NULLPTR);
  sp_io_write_str(&w.base, sp_str_lit("\u2590 "), SP_NULLPTR);
  sp_io_write_str(&w.base, name, SP_NULLPTR);
  sp_io_write_str(&w.base, sp_str_lit(SP_ANSI_RESET), SP_NULLPTR);

  if (padded_len > name.len) {
    sp_io_write_str(&w.base, sp_str_repeat(mem, pad, padded_len - name.len), SP_NULLPTR);
  }

  return sp_io_dyn_mem_writer_take_str(&w);
}

static sp_str_t spn_tui_name_to_color(sp_mem_t mem, sp_str_t str) {
  if (sp_str_equal_cstr(str, "package")) {
    return sp_str_lit(SP_ANSI_FG_WHITE);
  }

  static sp_ht(u32, sp_hash_t) buckets = SP_NULLPTR;
  if (!buckets) {
    sp_ht_init(spn.heap, buckets);
  }

  sp_hash_t hash = sp_hash_str(str);
  u32 lo = (u32)hash;

  static const f32 bucket_hues[] = {
    30, 40, 50, 60,
    160, 180,
    200, 220, 240,
    250, 280, 310, 340
  };

  u32 original_bucket = lo % sp_carr_len(bucket_hues);
  u32 bucket = original_bucket;
  while (sp_ht_key_exists(buckets, bucket)) {
    sp_hash_t* claimed = sp_ht_getp(buckets, bucket);
    if (claimed && *claimed == hash) {
      break;
    }
    bucket = (bucket + 1) % sp_carr_len(bucket_hues);
    if (bucket == original_bucket) {
      break;
    }
  }
  sp_ht_insert(buckets, bucket, hash);

  sp_color_t hsv = {
    .h = bucket_hues[bucket],
    .s = 40.0f,
    .v = 75.f,
  };
  sp_color_t rgb = sp_color_hsv_to_rgb(hsv);
  u8 r = (u8)(rgb.r * 255.0f);
  u8 g = (u8)(rgb.g * 255.0f);
  u8 b = (u8)(rgb.b * 255.0f);

  return sp_color_to_tui_rgb_f(mem, r, g, b);
}

static sp_str_t spn_tui_colored_name(sp_mem_t mem, sp_str_t name) {
  return sp_fmt(mem, "{}{}" SP_ANSI_RESET, sp_fmt_str(spn_tui_name_to_color(mem, name)), sp_fmt_str(name)).value;
}

static sp_str_t spn_tui_contextual_path(sp_mem_t mem, sp_str_t path) {
  if (!sp_str_empty(spn.paths.caches.dir) && sp_str_starts_with(path, spn.paths.caches.dir)) {
    sp_str_t rel = sp_str_strip_left(path, spn.paths.caches.dir);
    rel = sp_str_strip_left(rel, sp_str_lit("/"));
    return sp_fmt(mem, "$SPN_CACHE/{}", sp_fmt_str(rel)).value;
  }

  if (!sp_str_empty(spn.paths.project) && sp_str_starts_with(path, spn.paths.project)) {
    sp_str_t rel = sp_str_strip_left(path, spn.paths.project);
    rel = sp_str_strip_left(rel, sp_str_lit("/"));
    rel = sp_str_strip_left(rel, sp_str_lit("./"));
    return sp_fmt(mem, "./{}", sp_fmt_str(rel)).value;
  }

  return path;
}

static sp_str_t spn_tui_render_event_detail(sp_mem_t mem, spn_build_event_t* event) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &w);

  switch (event->kind) {
    case SPN_EVENT_SYNC: {
      sp_fmt_io(&w.base, "{} {.gray}", sp_fmt_str(event->sync.name), sp_fmt_str(event->sync.url));
      break;
    }
    case SPN_EVENT_SYNC_START: {
      sp_fmt_io(&w.base, "packages={} index={} file={}",
        SP_FMT_U32(event->sync_start.num_packages),
        SP_FMT_U32(event->sync_start.num_index),
        SP_FMT_U32(event->sync_start.num_file)
      );
      break;
    }
    case SPN_EVENT_SYNC_PACKAGE: {
      sp_fmt_io(&w.base, "{} fetched={} source={.gray}",
        sp_fmt_str(event->sync_pkg.name),
        sp_fmt_cstr(event->sync_pkg.fetched ? "true" : "false"),
        sp_fmt_str(event->sync_pkg.source_path)
      );
      break;
    }
    case SPN_EVENT_SYNC_END: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->sync_end.time);
      sp_fmt_io(&w.base, "synced {} packages in {.cyan}",
        SP_FMT_U32(event->sync_end.num_synced),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_PUBLISH: {
      sp_fmt_io(&w.base, "{} v{}",
        sp_fmt_str(event->publish.name),
        sp_fmt_str(event->publish.version)
      );
      break;
    }
    case SPN_EVENT_PUBLISH_END: {
      sp_fmt_io(&w.base, "{} v{} to {} {.gray}",
        sp_fmt_str(event->publish.name),
        sp_fmt_str(event->publish.version),
        sp_fmt_str(event->publish.index),
        sp_fmt_str(event->publish.url)
      );
      break;
    }
    case SPN_EVENT_COMPILE_START: {
      if (event->pkg) {
        sp_fmt_io(&w.base, "v{}", sp_fmt_str(spn_semver_to_str(mem, event->pkg->version)));
      }
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_COMPILE: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->script_compile.time);
      sp_fmt_io(&w.base, "{.gray} in {.gray}",
        sp_fmt_str(event->script_compile.script_path),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_USER_FN: {
      sp_fmt_io(&w.base, "{}", sp_fmt_str(event->node.info->tag));
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->package_ok.time);
      sp_fmt_io(&w.base, "in {.gray}", sp_fmt_cstr(buffer));
      break;
    }
    case SPN_EVENT_TARGET_BUILD_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->target.passed.time);
      sp_fmt_io(&w.base, "{.gray} in {.gray}",
        sp_fmt_str(event->target.passed.source_file),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_LINK_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->target.link_passed.time);
      sp_fmt_io(&w.base, "{.gray} in {.gray}",
        sp_fmt_str(event->target.link_passed.output_path),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_EMBED_START: {
      sp_fmt_io(&w.base, "{} files", SP_FMT_U32(event->embed_start.num_files));
      break;
    }
    case SPN_EVENT_EMBED_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->embed_passed.time);
      sp_fmt_io(&w.base, "{.gray} in {.gray}",
        sp_fmt_str(event->embed_passed.object_path),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_DIRTY_SUMMARY: {
      sp_fmt_io(&w.base, "commands={}/{} files={}/{} forced={}",
        SP_FMT_U32(event->dirty_summary.dirty_commands),
        SP_FMT_U32(event->dirty_summary.total_commands),
        SP_FMT_U32(event->dirty_summary.dirty_files),
        SP_FMT_U32(event->dirty_summary.total_files),
        sp_fmt_cstr(event->dirty_summary.forced ? "true" : "false")
      );
      break;
    }
    case SPN_EVENT_BUILD_SUMMARY: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->build_summary.time);
      sp_fmt_io(&w.base, "{}/{} commands in {.gray}",
        SP_FMT_U32(event->build_summary.num_dirty),
        SP_FMT_U32(event->build_summary.total_commands),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_RESOLVE_PACKAGE: {
      sp_fmt_io(&w.base, "{} {.gray}",
        sp_fmt_str(event->resolve_pkg.name),
        sp_fmt_str(event->resolve_pkg.version)
      );
      break;
    }
    case SPN_EVENT_RESOLVE_END: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->resolve_end.time);
      sp_fmt_io(&w.base, "Resolved {} packages in {.gray}",
        SP_FMT_U32(event->resolve_end.num_resolved),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_ADDED: {
      sp_fmt_io(&w.base, "{.cyan}=={.green}",
        sp_fmt_str(event->added.name),
        sp_fmt_str(event->added.version)
      );
      break;
    }
    case SPN_EVENT_TARGET_RUN: {
      sp_fmt_io(&w.base, "{.gray}", sp_fmt_str(event->run.command));
      break;
    }
    case SPN_EVENT_BUILD_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->build.passed.time);
      sp_fmt_io(&w.base,
        "Compiled for profile {.cyan} in {.gray}",
        sp_fmt_str(event->build.passed.profile->name),
        sp_fmt_cstr(buffer)
      );
      break;
    }
    case SPN_EVENT_BUILD_CANCELLED: {
      sp_fmt_io(&w.base, "profile {.cyan} with {} pending {}",
        sp_fmt_str(event->build_cancelled.profile),
        SP_FMT_U32(event->build_cancelled.num_pending),
        sp_fmt_cstr(event->build_cancelled.num_pending == 1 ? "command" : "commands")
      );
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_CRASHED: {
      if (sp_str_empty(event->crashed.error)) {
        sp_io_write_str(&w.base, sp_str_lit("build script crashed"), SP_NULLPTR);
      }
      else {
        sp_fmt_io(&w.base, "build script crashed: {.red}", sp_fmt_str(event->crashed.error));
      }
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED: {
      sp_fmt_io(&w.base, "{.cyan} failed to compile", sp_fmt_str(spn_tui_contextual_path(mem, event->compile_failed.script_path)));
      break;
    }
    case SPN_EVENT_TARGET_BUILD_FAILED: {
      sp_fmt_io(&w.base, "{.cyan} failed to compile", sp_fmt_str(spn_tui_contextual_path(mem, event->target.failed.source_file)));
      break;
    }
    case SPN_EVENT_ERR_UNKNOWN_PKG: {
      sp_fmt_io(
        &w.base,
        "{} could not be located",
        sp_fmt_str(spn_tui_colored_name(mem, event->unknown.request.qualified))
      );
      break;
    }
    case SPN_EVENT_ERR_UNSATISFIABLE_VERSION: {
      spn_evt_unsatisfiable_t* evt = &event->unsatisfiable;
      sp_str_t requester = sp_str_empty(evt->requester) ?
        sp_str_lit("the project") :
        sp_fmt(mem, "{} {}", sp_fmt_str(spn_tui_colored_name(mem, evt->requester)), sp_fmt_str(spn_semver_to_str(mem, evt->requester_version))).value;

      if (evt->conflict && evt->request.source == SPN_PKG_SOURCE_INDEX) {
        sp_fmt_io(
          &w.base,
          "{} is already selected at {.yellow}, but {} requires {.yellow}",
          sp_fmt_str(spn_tui_colored_name(mem, evt->request.qualified)),
          sp_fmt_str(spn_semver_to_str(mem, evt->selected)),
          sp_fmt_str(requester),
          sp_fmt_str(spn_semver_range_to_str(mem, evt->request.index.range))
        );
      }
      else if (evt->conflict) {
        sp_fmt_io(
          &w.base,
          "{} is already selected at {.yellow}, which conflicts with the version required by {}",
          sp_fmt_str(spn_tui_colored_name(mem, evt->request.qualified)),
          sp_fmt_str(spn_semver_to_str(mem, evt->selected)),
          sp_fmt_str(requester)
        );
      }
      else {
        sp_fmt_io(
          &w.base,
          "no version of {} satisfies {.yellow}, required by {}",
          sp_fmt_str(spn_tui_colored_name(mem, evt->request.qualified)),
          sp_fmt_str(spn_semver_range_to_str(mem, evt->request.index.range)),
          sp_fmt_str(requester)
        );
      }
      break;
    }
    case SPN_EVENT_ERR_CIRCULAR_DEP: {
      sp_fmt_io(
        &w.base,
        "{} transitively includes itself",
        sp_fmt_str(spn_tui_colored_name(mem, event->circular.id.name))
      );
      break;
    }
    case SPN_EVENT_ERR_UNIT_CYCLE: {
      sp_fmt_io(
        &w.base,
        "{} {.yellow} can't build: its build depends on a tool that links this same instance",
        sp_fmt_str(spn_tui_colored_name(mem, event->unit_cycle.id.name)),
        sp_fmt_str(spn_semver_to_str(mem, event->unit_cycle.version))
      );
      break;
    }
    case SPN_EVENT_ERR_DYNAMIC_DUPLICATE: {
      sp_fmt_io(
        &w.base,
        "{} {.yellow} and {.yellow} would both load into one process as shared libraries",
        sp_fmt_str(spn_tui_colored_name(mem, event->dynamic_dup.id.name)),
        sp_fmt_str(spn_semver_to_str(mem, event->dynamic_dup.low)),
        sp_fmt_str(spn_semver_to_str(mem, event->dynamic_dup.high))
      );
      break;
    }
    case SPN_EVENT_ERR_OPTION: {
      switch (event->option.err) {
        case SPN_OPTION_ERR_UNDECLARED: {
          sp_fmt_io(
            &w.base,
            "{} does not declare an option named {.yellow} (set by {.cyan})",
            sp_fmt_str(spn_tui_colored_name(mem, event->option.pkg)),
            sp_fmt_str(event->option.option),
            sp_fmt_str(spn_option_setter_to_str(event->option.a))
          );
          break;
        }
        case SPN_OPTION_ERR_BAD_VALUE: {
          sp_fmt_io(
            &w.base,
            "{.yellow} is not a valid value for {}.{.cyan} (set by {.cyan})",
            sp_fmt_str(event->option.value),
            sp_fmt_str(spn_tui_colored_name(mem, event->option.pkg)),
            sp_fmt_str(event->option.option),
            sp_fmt_str(spn_option_setter_to_str(event->option.a))
          );
          break;
        }
        case SPN_OPTION_ERR_CONFLICT: {
          sp_fmt_io(
            &w.base,
            "option conflict on {}.{.cyan}: {.cyan} and {.cyan} request different values",
            sp_fmt_str(spn_tui_colored_name(mem, event->option.pkg)),
            sp_fmt_str(event->option.option),
            sp_fmt_str(spn_option_setter_to_str(event->option.a)),
            sp_fmt_str(spn_option_setter_to_str(event->option.b))
          );
          break;
        }
        case SPN_OPTION_ERR_VETO: {
          sp_fmt_io(
            &w.base,
            "{.cyan} requires {}.{.cyan} != {.yellow}, but {} set it",
            sp_fmt_str(spn_option_setter_to_str(event->option.a)),
            sp_fmt_str(spn_tui_colored_name(mem, event->option.pkg)),
            sp_fmt_str(event->option.option),
            sp_fmt_str(event->option.value),
            sp_fmt_str(spn_option_setter_to_str(event->option.b))
          );
          break;
        }
        case SPN_OPTION_ERR_NO_VALUE: {
          sp_fmt_io(
            &w.base,
            "no value for {}.{.cyan}: no default matched and nothing set it",
            sp_fmt_str(spn_tui_colored_name(mem, event->option.pkg)),
            sp_fmt_str(event->option.option)
          );
          break;
        }
        case SPN_OPTION_ERR_LATE_GATE: {
          sp_fmt_io(
            &w.base,
            "the dependency gate on {}'s edge to {.cyan} never settled",
            sp_fmt_str(spn_tui_colored_name(mem, event->option.pkg)),
            sp_fmt_str(spn_option_setter_to_str(event->option.a))
          );
          break;
        }
        case SPN_OPTION_ERR_UNKNOWN_PKG: {
          sp_fmt_io(
            &w.base,
            "the root manifest configures {.yellow}, which is not a package in this build",
            sp_fmt_str(event->option.pkg)
          );
          break;
        }
      }
      break;
    }
    case SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX: {
      sp_fmt_io(
        &w.base,
        "resolving {} is too complex; pin a version to reduce the search",
        sp_fmt_str(spn_tui_colored_name(mem, event->too_complex.id.name))
      );
      break;
    }
    case SPN_EVENT_ERR_MANIFEST: {
      sp_fmt_io(
        &w.base,
        "{} has an invalid manifest ({.gray})",
        sp_fmt_str(spn_tui_colored_name(mem, event->manifest_err.name)),
        sp_fmt_str(spn_tui_contextual_path(mem, event->manifest_err.path))
      );
      if (sp_da_empty(event->manifest_err.issues)) {
        sp_fmt_io(&w.base, ": {}", sp_fmt_str(event->manifest_err.error));
      }
      break;
    }
    case SPN_EVENT_SYNC_FAILED: {
      sp_fmt_io(
        &w.base,
        "{} failed to sync from {.gray}: {}",
        sp_fmt_str(spn_tui_colored_name(mem, event->sync_failed.name)),
        sp_fmt_str(event->sync_failed.url),
        sp_fmt_str(event->sync_failed.error)
      );
      break;
    }
    case SPN_EVENT_SYNC_STALE: {
      sp_fmt_io(
        &w.base,
        "{} could not be fetched from {.gray}; using the cached copy",
        sp_fmt_str(spn_tui_colored_name(mem, event->sync.name)),
        sp_fmt_str(event->sync.url)
      );
      break;
    }
    case SPN_EVENT_ERR: {
      switch (event->err.kind) {
        // case SPN_ERR_OK: {
        //   break;
        // }
        case SPN_ERR_MANIFEST_PARSE: {
          sp_fmt_io(
            &w.base,
            "failed to parse manifest {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.manifest_parse.path))
          );
          break;
        }
        case SPN_ERR_MANIFEST_FIELD: {
          sp_fmt_io(
            &w.base,
            "invalid manifest field {.cyan}: expected {.yellow}, got {.red}",
            sp_fmt_str(event->err.manifest_field.path),
            sp_fmt_str(event->err.manifest_field.expected),
            sp_fmt_str(event->err.manifest_field.actual)
          );
          break;
        }
        case SPN_ERR_MANIFEST_ISSUES: {
          sp_io_write_str(&w.base, sp_str_lit("invalid manifest"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_PROFILE_INVALID: {
          sp_fmt_io(
            &w.base,
            "invalid profile {.cyan}",
            sp_fmt_str(event->err.profile.name)
          );
          break;
        }
        case SPN_ERR_PROFILE_UNDEFINED: {
          sp_fmt_io(
            &w.base,
            "profile {.cyan} isn't defined",
            sp_fmt_str(event->err.profile.name)
          );
          break;
        }
        case SPN_ERR_FLAG_INVALID: {
          sp_fmt_io(
            &w.base,
            "invalid value {.red} for {.yellow}; expected {}",
            sp_fmt_str(event->err.flag.value),
            sp_fmt_str(event->err.flag.name),
            sp_fmt_str(event->err.flag.expected)
          );
          break;
        }
        case SPN_ERR_SANITIZER_UNSUPPORTED: {
          sp_fmt_io(
            &w.base,
            "toolchain {.cyan} targeting {.yellow} doesn't support {.red}",
            sp_fmt_str(event->err.sanitizer.toolchain),
            sp_fmt_str(spn_triple_to_str(mem, event->err.sanitizer.target)),
            sp_fmt_str(spn_sanitizer_set_to_str(mem, event->err.sanitizer.unsupported))
          );
          break;
        }
        case SPN_ERR_SANITIZER_STATIC: {
          sp_fmt_io(
            &w.base,
            "{.red} requires a dynamically linked executable; set {.cyan} in the profile",
            sp_fmt_str(spn_sanitizer_set_to_str(mem, event->err.sanitizer.unsupported)),
            sp_fmt_str(sp_str_lit("linkage = \"shared\""))
          );
          break;
        }
        case SPN_ERR_COMPILER_FEATURE_UNSUPPORTED: {
          sp_fmt_io(
            &w.base,
            "toolchain {.cyan} targeting {.yellow} doesn't support {.red}",
            sp_fmt_str(event->err.compiler.toolchain),
            sp_fmt_str(spn_triple_to_str(mem, event->err.compiler.target)),
            sp_fmt_str(spn_cc_feature_to_str(event->err.compiler.feature))
          );
          break;
        }
        case SPN_ERR_FS_REMOVE: {
          sp_fmt_io(
            &w.base,
            "failed to remove {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_WASM_INIT_FAILED: {
          sp_io_write_str(&w.base, sp_str_lit("failed to initialize the wasm runtime"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_BUILD_GRAPH: {
          sp_fmt_io(
            &w.base,
            "failed to construct the build graph at {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.build_graph.file))
          );
          break;
        }
        case SPN_ERR_FS_READ: {
          sp_fmt_io(
            &w.base,
            "failed to read {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_FS_WRITE: {
          sp_fmt_io(
            &w.base,
            "failed to write {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.fs.path))
          );
          break;
        }
        case SPN_ERR_INDEX_UNKNOWN: {
          sp_fmt_io(
            &w.base,
            "index {.cyan} not found",
            sp_fmt_str(event->err.index.name)
          );
          break;
        }
        case SPN_ERR_INDEX_SYNC: {
          sp_fmt_io(
            &w.base,
            "failed to sync index {.cyan} from {.gray}",
            sp_fmt_str(event->err.index.name),
            sp_fmt_str(event->err.index.url)
          );
          break;
        }
        case SPN_ERR_PKG_UNKNOWN: {
          sp_fmt_io(
            &w.base,
            "package {.cyan} not found in any index",
            sp_fmt_str(event->err.pkg.name)
          );
          break;
        }
        case SPN_ERR_PKG_NO_MATCH: {
          sp_fmt_io(
            &w.base,
            "no version of {.cyan} matches {.red}",
            sp_fmt_str(event->err.pkg.name),
            sp_fmt_str(sp_str_empty(event->err.pkg.requested) ? sp_str_lit("*") : event->err.pkg.requested)
          );
          break;
        }
        case SPN_ERR_MANIFEST_EDIT: {
          sp_fmt_io(
            &w.base,
            "failed to edit {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.manifest_parse.path))
          );
          break;
        }
        case SPN_ERR_NO_MANIFEST: {
          sp_fmt_io(
            &w.base,
            "no manifest found at {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.no_manifest.path))
          );
          break;
        }
        case SPN_ERR_NOT_GIT_REPO: {
          sp_fmt_io(
            &w.base,
            "{.cyan} is not inside a git repository",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.not_git_repo.path))
          );
          break;
        }
        case SPN_ERR_GIT: {
          sp_fmt_io(
            &w.base,
            "git command failed: {.yellow}",
            sp_fmt_str(event->err.git.command)
          );
          break;
        }
        case SPN_ERR_VERSION_EXISTS: {
          sp_fmt_io(
            &w.base,
            "version {.yellow} of {.cyan} already exists in the index",
            sp_fmt_str(event->err.version_exists.version),
            sp_fmt_str(event->err.version_exists.name)
          );
          break;
        }
        case SPN_ERR_INDEX_PINNED: {
          sp_fmt_io(
            &w.base,
            "index {.cyan} is pinned to a revision and cannot be published to",
            sp_fmt_str(event->err.index.name)
          );
          break;
        }
        case SPN_ERR_INDEX_PUBLISH_PROTOCOL: {
          sp_fmt_io(
            &w.base,
            "index {.cyan} does not support publishing",
            sp_fmt_str(event->err.index.name)
          );
          break;
        }
        case SPN_ERR_PUBLISH_PUSH: {
          sp_fmt_io(
            &w.base,
            "failed to push to {.gray}\n{}",
            sp_fmt_str(event->err.publish.url),
            sp_fmt_str(event->err.publish.output)
          );
          break;
        }
        case SPN_ERR_PUBLISH_DIRTY: {
          sp_fmt_io(
            &w.base,
            "{.cyan} has uncommitted changes; commit them or pass {.yellow}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.publish.path)),
            sp_fmt_cstr("--allow-dirty")
          );
          break;
        }
        case SPN_ERR_PUBLISH_UNPUSHED: {
          sp_fmt_io(
            &w.base,
            "commit {.yellow} is not on any branch of {.gray}; push it first",
            sp_fmt_str(event->err.publish.rev),
            sp_fmt_str(event->err.publish.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_FETCH: {
          sp_fmt_io(
            &w.base,
            "toolchain {} failed to download from {.gray}",
            sp_fmt_str(spn_tui_colored_name(mem, event->err.artifact.name)),
            sp_fmt_str(event->err.artifact.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_NO_SHA: {
          sp_fmt_io(
            &w.base,
            "toolchain {} has no sha256 for {.gray}",
            sp_fmt_str(spn_tui_colored_name(mem, event->err.artifact.name)),
            sp_fmt_str(event->err.artifact.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_SHA: {
          sp_fmt_io(
            &w.base,
            "toolchain {} sha256 mismatch for {.gray}: expected {.yellow}, got {.red}",
            sp_fmt_str(spn_tui_colored_name(mem, event->err.artifact.name)),
            sp_fmt_str(event->err.artifact.url),
            sp_fmt_str(event->err.artifact.expected),
            sp_fmt_str(event->err.artifact.actual)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_EXTRACT: {
          sp_fmt_io(
            &w.base,
            "toolchain {} failed to extract archive from {.gray}",
            sp_fmt_str(spn_tui_colored_name(mem, event->err.artifact.name)),
            sp_fmt_str(event->err.artifact.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_UNKNOWN: {
          sp_fmt_io(
            &w.base,
            "toolchain {} isn't defined",
            sp_fmt_str(spn_tui_colored_name(mem, event->err.toolchain.name))
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_TARGET: {
          sp_str_t target = spn_triple_to_str(mem, event->err.toolchain.target);
          switch (event->err.toolchain.role) {
            case SPN_TOOLCHAIN_ROLE_BUILD: {
              sp_fmt_io(
                &w.base,
                "toolchain {} can't target {.yellow}",
                sp_fmt_str(spn_tui_colored_name(mem, event->err.toolchain.name)),
                sp_fmt_str(target)
              );
              break;
            }
            case SPN_TOOLCHAIN_ROLE_SCRIPT: {
              sp_fmt_io(
                &w.base,
                "build scripts compile to {.yellow}, but toolchain {} can't target it",
                sp_fmt_str(target),
                sp_fmt_str(spn_tui_colored_name(mem, event->err.toolchain.name))
              );
              break;
            }
          }
          break;
        }
        case SPN_ERR_TOOLCHAIN_HOST: {
          sp_str_t host = spn_triple_to_str(mem, event->err.toolchain.host);
          sp_fmt_io(
            &w.base,
            "toolchain {} isn't distributed for host {.yellow}",
            sp_fmt_str(spn_tui_colored_name(mem, event->err.toolchain.name)),
            sp_fmt_str(host)
          );
          break;
        }
        case SPN_ERR_WASM_READ_FAILED: {
          sp_fmt_io(
            &w.base,
            "failed to read build script {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_WASM_MODULE_LOAD_FAILED: {
          sp_fmt_io(
            &w.base,
            "failed to load build script {.cyan}: {.red}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.wasm.path)),
            sp_fmt_str(event->err.wasm.error)
          );
          break;
        }
        case SPN_ERR_WASM_MODULE_INSTANCE_FAILED: {
          sp_fmt_io(
            &w.base,
            "failed to instantiate build script {.cyan}: {.red}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.wasm.path)),
            sp_fmt_str(event->err.wasm.error)
          );
          break;
        }
        case SPN_ERR_WASM_THREAD_ENV_FAILED: {
          sp_fmt_io(
            &w.base,
            "failed to init wasm thread env for build script {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_WASM_CTX_FAILED: {
          sp_fmt_io(
            &w.base,
            "failed to create wasm context for build script {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_WASM_MODULE_CALL_FAILED: {
          sp_fmt_io(
            &w.base,
            "build script {.cyan} crashed: {.red}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.wasm.path)),
            sp_fmt_str(event->err.wasm.error)
          );
          break;
        }
        case SPN_ERR_WASM_SCRIPT_ERROR: {
          sp_fmt_io(
            &w.base,
            "build script {.cyan} returned {.red}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.wasm.path)),
            SP_FMT_S32(event->err.wasm.rc)
          );
          break;
        }
        case SPN_ERR_WASM_NO_SCRIPT: {
          sp_io_write_str(&w.base, sp_str_lit("node has a wasm fn but no build script is loaded"), SP_NULLPTR);
          break;
        }
        case SPN_ERR_WASM_EXPORT_NOT_FOUND: {
          sp_fmt_io(
            &w.base,
            "Referenced symbol {.yellow} was not found in {.cyan}",
            sp_fmt_str(event->err.wasm.error),
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.wasm.path))
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_NO_CXX: {
          sp_fmt_io(
            &w.base,
            "Toolchain {} has no C++ compiler, but the build contains C++ sources",
            sp_fmt_str(spn_tui_colored_name(mem, event->err.toolchain.name))
          );
          break;
        }
        default: {
          sp_io_write_str(&w.base, sp_str_lit("unknown error"), SP_NULLPTR);
          break;
        }
      }
      break;
    }
    case SPN_EVENT_USER_LOG: {
      sp_io_write_str(&w.base, event->user_log.message, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_API_CALL: {
      sp_fmt_io(&w.base, "{}({})",
        sp_fmt_str(event->api_call.fn),
        sp_fmt_str(event->api_call.args)
      );
      break;
    }
    case SPN_EVENT_INIT_BUILD_GRAPH: {
      sp_fmt_io(&w.base, "profile={} force={} packages={}",
        sp_fmt_str(event->graph_init.profile),
        sp_fmt_cstr(event->graph_init.force ? "true" : "false"),
        SP_FMT_U32(event->graph_init.packages)
      );
      break;
    }
    case SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED: {
      switch (event->err.build_graph.kind) {
        case SPN_BUILD_GRAPH_ERR_MISSING_INPUT: {
          sp_fmt_io(
            &w.base,
            "Missing build graph input {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.build_graph.file))
          );
          break;
        }
        case SPN_BUILD_GRAPH_ERR_DUPLICATE_OUTPUT: {
          sp_fmt_io(
            &w.base,
            "Two graph nodes output the same file {.cyan}",
            sp_fmt_str(spn_tui_contextual_path(mem, event->err.build_graph.file))
          );
          break;
        }
        case SPN_BUILD_GRAPH_ERR_UNKNOWN: {
          sp_io_write_str(&w.base, sp_str_lit("Failed to prepare build graph"), SP_NULLPTR);
          break;
        }
      }
      break;
    }
    case SPN_EVENT_LINK_START: {
      sp_fmt_io(&w.base, "Linking target {.cyan}",
        sp_fmt_str(event->target.name)
      );
      break;
    }
    case SPN_EVENT_LINK_FAILED: {
      sp_fmt_io(&w.base, "Failed to link target {.cyan}", sp_fmt_str(event->target.name));
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->configure.time);
      sp_fmt_io(&w.base, "in {.gray}", sp_fmt_cstr(buffer));
      break;
    }
    case SPN_EVENT_EMBED_FAILED: {
      sp_fmt_io(&w.base, "{}: {}",
        sp_fmt_str(spn_tui_contextual_path(mem, event->embed_failed.path)),
        sp_fmt_str(event->embed_failed.error)
      );
      break;
    }
    case SPN_EVENT_BUILD_FAILED: {
      sp_fmt_io(&w.base, "profile {.cyan} failed with {} {}",
        sp_fmt_str(event->build_failed.profile),
        SP_FMT_U32(event->build_failed.num_errors),
        sp_fmt_cstr(event->build_failed.num_errors == 1 ? "error" : "errors")
      );
      if (!sp_str_empty(event->build_failed.first_error)) {
        sp_fmt_io(&w.base, " ({})", sp_fmt_str(event->build_failed.first_error));
      }
      break;
    }
    default: {
      break;
    }
  }

  return sp_io_dyn_mem_writer_take_str(&w);
}

static sp_str_t spn_tui_short_name(sp_str_t qualified) {
  sp_for(it, qualified.len) {
    u32 index = qualified.len - it - 1;
    if (qualified.data[index] == '/') {
      return sp_str_sub(qualified, index + 1, qualified.len - index - 1);
    }
  }
  return qualified;
}

static void spn_tui_write_event_tail(sp_io_writer_t* w, sp_mem_t mem, sp_str_t pkg_name, sp_str_t detail) {
  if (sp_str_empty(pkg_name)) {
    sp_fmt_io(w, " {.gray}", sp_fmt_cstr("▐"));
  } else {
    sp_fmt_io(w, " {}", sp_fmt_str(spn_tui_decorate_name(mem, pkg_name, 0, ' ')));
  }
  if (!sp_str_empty(detail)) {
    sp_fmt_io(w, " {}", sp_fmt_str(detail));
  }
  sp_io_write_c8(w, '\n');
}

static void spn_tui_write_event_line(sp_io_writer_t* w, sp_mem_t mem, sp_str_t verb, bool error, sp_str_t pkg_name, sp_str_t detail) {
  sp_fmt_io(w, "{:>12 .bold .$}", sp_fmt_style(error ? sp_fmt_style_red : sp_fmt_style_green), sp_fmt_str(verb));
  spn_tui_write_event_tail(w, mem, pkg_name, detail);
}

static void spn_tui_write_terminal_error_line(sp_io_writer_t* w, sp_mem_t mem, sp_str_t verb, sp_str_t detail) {
  sp_str_t label = sp_fmt(mem, "{}:", sp_fmt_str(verb)).value;
  sp_fmt_io(w, "{.bold .red}", sp_fmt_str(label));
  if (!sp_str_empty(detail)) {
    sp_fmt_io(w, " {}", sp_fmt_str(detail));
  }
  sp_io_write_c8(w, '\n');
}

static sp_str_t spn_tui_event_subject(spn_build_event_t* event) {
  switch (event->kind) {
    case SPN_EVENT_ERR_MANIFEST:              return event->manifest_err.name;
    case SPN_EVENT_SYNC_FAILED:               return event->sync_failed.name;
    case SPN_EVENT_ERR_UNKNOWN_PKG:           return event->unknown.request.qualified;
    case SPN_EVENT_ERR_CIRCULAR_DEP:          return event->circular.id.name;
    case SPN_EVENT_ERR_UNSATISFIABLE_VERSION: return event->unsatisfiable.request.qualified;
    case SPN_EVENT_ERR_UNIT_CYCLE:            return event->unit_cycle.id.name;
    case SPN_EVENT_ERR_DYNAMIC_DUPLICATE:     return event->dynamic_dup.id.name;
    case SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX: return event->too_complex.id.name;
    default:                                  return event->pkg ? event->pkg->name : sp_str_lit("");
  }
}

static void spn_tui_render_event_extra(sp_io_writer_t* w, spn_build_event_t* event) {
  switch (event->kind) {
    case SPN_EVENT_TARGET_BUILD_FAILED: {
      sp_io_write_str(w, event->target.failed.out, SP_NULLPTR);
      sp_io_write_str(w, event->target.failed.err, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_LINK_FAILED: {
      sp_io_write_str(w, event->target.link_failed.err, SP_NULLPTR);
      sp_io_write_str(w, event->target.link_failed.out, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED: {
      sp_io_write_str(w, event->compile_failed.error, SP_NULLPTR);
      break;
    }
    case SPN_EVENT_ERR_MANIFEST: {
      sp_da_for(event->manifest_err.issues, it) {
        sp_io_write_str(w, sp_str_lit("  - "), SP_NULLPTR);
        spn_codegen_issue_write(w, &event->manifest_err.issues[it]);
        sp_io_write_c8(w, '\n');
      }
      break;
    }
    case SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED: {
      if (event->err.build_graph.kind == SPN_BUILD_GRAPH_ERR_DUPLICATE_OUTPUT) {
        if (sp_str_valid(event->err.build_graph.command_a)) {
          sp_io_write_str(w, event->err.build_graph.command_a, SP_NULLPTR);
          sp_io_write_c8(w, '\n');
        }
        if (sp_str_valid(event->err.build_graph.command_b)) {
          sp_io_write_str(w, event->err.build_graph.command_b, SP_NULLPTR);
          sp_io_write_c8(w, '\n');
        }
      }
      break;
    }
    case SPN_EVENT_ERR: {
      switch (event->err.kind) {
        case SPN_ERR_MANIFEST_ISSUES: {
          sp_da_for(event->err.issues, it) {
            sp_io_write_str(w, sp_str_lit("  - "), SP_NULLPTR);
            spn_codegen_issue_write(w, &event->err.issues[it]);
            sp_io_write_c8(w, '\n');
          }
          break;
        }
        case SPN_ERR_TOOLCHAIN_TARGET: {
          bool first = true;
          sp_str_ht_for_kv(event->err.toolchain.catalog->entries, it) {
            spn_toolchain_info_t* toolchain = *it.val;
            if (!spn_toolchain_supports(toolchain, event->err.toolchain.target, event->err.toolchain.host)) continue;
            sp_io_write_str(w, first ? sp_str_lit("toolchains that can: ") : sp_str_lit(", "), SP_NULLPTR);
            sp_fmt_io(w, "{.green}", sp_fmt_str(toolchain->name));
            first = false;
          }
          if (!first) {
            sp_io_write_c8(w, '\n');
          }
          break;
        }
        default: {
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }
}

typedef struct {
  sp_str_t pkg;
  sp_str_t message;
} spn_tui_buffered_log_t;

void spn_tui_log_event(spn_build_event_t* event) {
  static sp_str_ht(bool) seen_url = SP_NULLPTR;
  static sp_da(spn_tui_buffered_log_t) buffered_logs = SP_NULLPTR;
  static u32 num_downloads = 0;
  if (!seen_url) {
    sp_str_ht_init(spn.heap, seen_url);
    sp_da_init(spn.heap, buffered_logs);
  }

  const spn_event_info_t* display = &spn_event_info[event->kind];
  if (display->verbosity > spn.logger.verbosity) {
    if (event->kind == SPN_EVENT_USER_LOG) {
      sp_da_push(buffered_logs, ((spn_tui_buffered_log_t) {
        .pkg = event->pkg ? sp_str_copy(spn.heap, event->pkg->name) : sp_str_lit(""),
        .message = sp_str_copy(spn.heap, event->user_log.message),
      }));
    }
    return;
  }

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_mem_t mem = scratch.mem;
  sp_io_writer_t* w = spn.tui.out;
  sp_str_t verb = sp_str_view(display->display);

  if (display->error) {
    sp_da_for(buffered_logs, it) {
      spn_tui_buffered_log_t* log = &buffered_logs[it];
      spn_tui_write_event_line(w, mem, sp_str_lit(""), false, log->pkg, log->message);
    }
    sp_da_clear(buffered_logs);

    sp_str_t detail = spn_tui_render_event_detail(mem, event);
    if (display->terminal) {
      spn_tui_write_event_line(w, mem, sp_str_lit("Failed"), true, spn_tui_event_subject(event), sp_str_lit(""));
      sp_io_write_c8(w, '\n');
      spn_tui_write_terminal_error_line(w, mem, verb, detail);
    } else {
      sp_str_t name = event->pkg ? event->pkg->name : sp_str_lit("");
      spn_tui_write_event_line(w, mem, verb, true, name, detail);
    }
    spn_tui_render_event_extra(w, event);
    spn_tui_line_writer_flush(&spn.tui.line_writer);
    sp_mem_end_scratch(scratch);
    return;
  }

  switch (event->kind) {
    case SPN_EVENT_SYNC: {
      if (!sp_str_ht_get(seen_url, event->sync.url)) {
        sp_str_ht_insert(seen_url, sp_str_copy(spn.heap, event->sync.url), true);
        num_downloads++;
        spn_tui_write_event_line(
          w, mem, verb, false,
          spn_tui_short_name(event->sync.name),
          sp_fmt(mem, "{.gray}", sp_fmt_str(event->sync.url)).value
        );
      }
      break;
    }

    case SPN_EVENT_SYNC_END: {
      if (!num_downloads) break;

      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->sync_end.time);
      spn_tui_write_event_line(
        w, mem, verb, false,
        sp_str_lit(""),
        sp_fmt(mem, "{} {} in {.gray}",
          SP_FMT_U32(num_downloads),
          sp_fmt_cstr(num_downloads == 1 ? "package" : "packages"),
          sp_fmt_cstr(buffer)
        ).value
      );
      break;
    }

    case SPN_EVENT_RESOLVE_END:
    case SPN_EVENT_BUILD_PASSED: {
      spn_tui_write_event_line(w, mem, verb, false, sp_str_lit(""), spn_tui_render_event_detail(mem, event));
      break;
    }

    case SPN_EVENT_TARGET_RUN: {
      sp_str_t name = event->run.name;
      if (sp_str_empty(name) && event->pkg) {
        name = event->pkg->name;
      }
      spn_tui_write_event_line(w, mem, verb, false, name, spn_tui_render_event_detail(mem, event));
      break;
    }

    case SPN_EVENT_USER_LOG: {
      sp_str_t name = event->pkg ? event->pkg->name : sp_str_lit("");
      spn_tui_write_event_line(w, mem, sp_str_lit(""), false, name, event->user_log.message);
      break;
    }

    default: {
      sp_str_t name = event->pkg ? event->pkg->name : sp_str_lit("");
      spn_tui_write_event_line(w, mem, verb, false, name, spn_tui_render_event_detail(mem, event));
      break;
    }
  }

  spn_tui_line_writer_flush(&spn.tui.line_writer);
  sp_mem_end_scratch(scratch);
}

static void spn_tui_line_writer_emit(spn_tui_line_writer_t* writer, sp_str_t line) {
  if (writer->prompt) {
    sp_prompt_log_str(writer->prompt, line);
  } else {
    sp_io_write_str(writer->downstream, line, SP_NULLPTR);
    sp_io_write_c8(writer->downstream, '\n');
  }
}

static void spn_tui_line_writer_complete_line(spn_tui_line_writer_t* writer) {
  sp_str_t line = sp_str_trim_right(sp_str(writer->partial, sp_da_size(writer->partial)));

  if (sp_str_empty(line)) {
    writer->deferred_blanks++;
  } else {
    sp_for(it, writer->deferred_blanks) {
      spn_tui_line_writer_emit(writer, sp_str_lit(""));
    }
    writer->deferred_blanks = 0;
    spn_tui_line_writer_emit(writer, line);
  }

  sp_da_clear(writer->partial);
}

static sp_err_t spn_tui_line_writer_write(sp_io_writer_t* w, const void* ptr, u64 size, u64* bytes_written) {
  spn_tui_line_writer_t* writer = (spn_tui_line_writer_t*)w;
  const c8* bytes = (const c8*)ptr;

  sp_for(it, size) {
    c8 c = bytes[it];
    if (c == '\n') {
      spn_tui_line_writer_complete_line(writer);
    } else {
      sp_da_push(writer->partial, c);
    }
  }

  if (bytes_written) {
    *bytes_written = size;
  }
  return SP_OK;
}

static void spn_tui_line_writer_flush(spn_tui_line_writer_t* writer) {
  if (!sp_da_empty(writer->partial)) {
    spn_tui_line_writer_complete_line(writer);
  }
  writer->deferred_blanks = 0;
}

void spn_tui_attach_prompt(spn_tui_t* tui, sp_prompt_ctx_t* ctx) {
  spn_tui_line_writer_flush(&tui->line_writer);
  tui->line_writer.prompt = ctx;
}

void spn_tui_detach_prompt(spn_tui_t* tui) {
  tui->line_writer.prompt = SP_NULLPTR;
  spn_tui_line_writer_flush(&tui->line_writer);
}

void spn_tui_init(spn_tui_t* tui, spn_session_t* session, spn_tui_mode_t mode) {
  tui->mode = mode;
  tui->session = session;
  tui->line_writer = (spn_tui_line_writer_t) {
    .base.write = spn_tui_line_writer_write,
    .downstream = &spn.logger.err.base,
  };
  sp_da_init(spn.heap, tui->line_writer.partial);
  tui->out = &tui->line_writer.base;

  switch (tui->mode) {
    case SPN_OUTPUT_MODE_INTERACTIVE: {
      break;
    }
    case SPN_OUTPUT_MODE_QUIET:
    case SPN_OUTPUT_MODE_NONINTERACTIVE:
    case SPN_OUTPUT_MODE_NONE: {
      break;
    }
  }
}

static void spn_prompt_on_event(sp_prompt_ctx_t* ctx, sp_prompt_event_t event) {
  switch (event.kind) {
    case SP_PROMPT_EVENT_CTRL_C:
    case SP_PROMPT_EVENT_ESCAPE: {
      break;
    }
    default: {
      spn.tui.prompt.widget.on_event(ctx, event);
      break;
    }
  }
}

static void spn_prompt_start(void) {
  spn_tui_t* tui = &spn.tui;
  if (tui->prompt.started) return;
  tui->prompt.started = true;

  if (tui->mode != SPN_OUTPUT_MODE_INTERACTIVE) return;
  if (!sp_os_is_tty(sp_sys_stdout)) return;

  tui->prompt.ctx = sp_prompt_begin(spn.mem);
  if (!tui->prompt.ctx) return;

  tui->prompt.widget = sp_prompt_progress_widget(tui->prompt.ctx, (sp_prompt_progress_t) {
    .prompt = "Building",
    .color = { .rgb = { .r = 99, .g = 160, .b = 136 } },
  });
  sp_prompt_widget_t widget = tui->prompt.widget;
  widget.on_event = spn_prompt_on_event;
  sp_prompt_app(tui->prompt.ctx, widget);
  tui->prompt.app = (sp_app_t) { .user_data = tui->prompt.ctx };
  sp_prompt_app_on_init(&tui->prompt.app);
  tui->prompt.on = true;
  spn_tui_attach_prompt(tui, tui->prompt.ctx);
}

// @spader
// What does this even mean? What does it mean to "submit" the main
// TUI? Also, this can get called from:
// - poll(), when we see the shutdown signal
// - update(), if a step returns an error
// - deinit(), unconditionally in interactive mode?
//
//
void spn_prompt_stop(bool ok) {
  spn_tui_t* tui = &spn.tui;
  if (!tui->prompt.on) return;

  sp_prompt_state_t state = ok ? SP_PROMPT_STATE_SUBMIT : SP_PROMPT_STATE_ERROR;
  if (sp_atomic_s32_get(&spn.aborted)) {
    state = SP_PROMPT_STATE_CANCEL;
  }

  sp_prompt_set_state(tui->prompt.ctx, state);
  sp_prompt_app_on_poll(&tui->prompt.app);
  sp_prompt_end(tui->prompt.ctx);
  tui->prompt.on = false;
  spn_tui_detach_prompt(tui);
}

// @spader
//
void spn_prompt_pump() {
  spn_tui_t* tui = &spn.tui;

  if (sp_atomic_s32_get(&spn.aborted)) {
    spn_prompt_stop(false);
    return;
  }

  spn_dag_build_t* dag = tui->session->dag;
  if (!dag) return;

  if (!tui->prompt.on) {
    if (!sp_atomic_s32_get(&dag->progress.executed)) return;
    spn_prompt_start();
    if (!tui->prompt.on) return;
  }
  u32 total = (u32)sp_atomic_s32_get(&dag->progress.total);
  u32 done = (u32)sp_atomic_s32_get(&dag->progress.completed);

  f32 value = total ? (f32)done / (f32)total : 0.f;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_prompt_send_status_str(tui->prompt.ctx, sp_fmt(s.mem,
    "{}/{} units", sp_fmt_uint(done), sp_fmt_uint(total)).value);
  sp_mem_end_scratch(s);

  sp_prompt_send_progress_f32(tui->prompt.ctx, value);
  sp_prompt_app_on_poll(&tui->prompt.app);
}
