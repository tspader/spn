#include "profile/types.h"

#include "codegen/codegen.h"
#include "ctx/ctx.h"
#include "log/log.h"
#include "semver/convert.h"
#include "sp/color.h"
#include "sp/io.h"
#include "sp/macro.h"
#include "sp/str.h"
#include "toolchain/select.h"
#include "triple/triple.h"
#include "tui/tui.h"

#include <stdarg.h>
#include <stdio.h>

#ifdef _WIN32
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

#define ERROR_VERB     true
#define NOT_ERROR_VERB false

typedef struct {
  const c8* name;
  const c8* id;
  spn_verbosity_t verbosity;
  bool error;
} spn_build_event_display_t;

#define EVENT(ID, NAME, VERBOSITY, ERROR) [ID] = { NAME, sp_mstr(ID), VERBOSITY, ERROR }

static spn_build_event_display_t event_info[] = {
  EVENT(SPN_EVENT_ERR,                           "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_ERR_CIRCULAR_DEP,              "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_ERR_UNKNOWN_PKG,               "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_ERR_UNSATISFIABLE_VERSION,     "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_ERR_MANIFEST,                  "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_RESOLVE_PACKAGE,               "Resolving",   SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_RESOLVE_END,                   "Resolved",    SPN_VERBOSITY_NORMAL,  NOT_ERROR_VERB),
  EVENT(SPN_EVENT_SYNC,                          "Downloading", SPN_VERBOSITY_NORMAL,  NOT_ERROR_VERB),
  EVENT(SPN_EVENT_SYNC_START,                    "Syncing",     SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_SYNC_PACKAGE,                  "Synced",      SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_SYNC_FAILED,                   "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_SYNC_END,                      "Downloaded",  SPN_VERBOSITY_NORMAL,  NOT_ERROR_VERB),
  EVENT(SPN_EVENT_BUILD_SCRIPT_COMPILE,          "Compiling",   SPN_VERBOSITY_VERBOSE, NOT_ERROR_VERB),
  EVENT(SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED,   "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_BUILD_SCRIPT_CONFIGURE,        "Configuring", SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK,     "Configured",  SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_BUILD_SCRIPT_PACKAGE,          "Packaging",   SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK,       "Packaged",    SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_BUILD_SCRIPT_CRASHED,          "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_BUILD_SCRIPT_USER_FN,          "Running",     SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_COMPILE_START,                 "Compiling",   SPN_VERBOSITY_NORMAL,  NOT_ERROR_VERB),
  EVENT(SPN_EVENT_TARGET_BUILD_PASSED,           "Compiled",    SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_TARGET_BUILD_FAILED,           "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_TARGET_RUN,                    "Running",     SPN_VERBOSITY_NORMAL,  NOT_ERROR_VERB),
  EVENT(SPN_EVENT_LINK_START,                    "Linking",     SPN_VERBOSITY_VERBOSE, NOT_ERROR_VERB),
  EVENT(SPN_EVENT_LINK_PASSED,                   "Linked",      SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_LINK_FAILED,                   "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_EMBED_START,                   "Embedding",   SPN_VERBOSITY_VERBOSE, NOT_ERROR_VERB),
  EVENT(SPN_EVENT_EMBED_PASSED,                  "Embedded",    SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_EMBED_FAILED,                  "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_INIT_BUILD_GRAPH,              "Planning",    SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED,    "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_DIRTY_SUMMARY,                 "Planned",     SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_BUILD_PASSED,                  "Finished",    SPN_VERBOSITY_NORMAL,  NOT_ERROR_VERB),
  EVENT(SPN_EVENT_BUILD_FAILED,                  "error",       SPN_VERBOSITY_QUIET,   ERROR_VERB    ),
  EVENT(SPN_EVENT_BUILD_SUMMARY,                 "Summary",     SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_API_CALL,                      "Calling",     SPN_VERBOSITY_DEBUG,   NOT_ERROR_VERB),
  EVENT(SPN_EVENT_USER_LOG,                      "",            SPN_VERBOSITY_VERBOSE, NOT_ERROR_VERB),
};

static sp_str_t spn_tui_name_to_color(sp_mem_t mem, sp_str_t str);
static sp_str_t spn_tui_decorate_name(sp_mem_t mem, sp_str_t name, u32 padded_len, c8 pad);

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

  SP_FATAL("Unknown output mode {.yellow}; options are [interactive, noninteractive, quiet, none]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_OUTPUT_MODE_NONE);
}

sp_str_t spn_output_mode_to_str(spn_tui_mode_t mode) {
  switch (mode) {
    SPN_OUTPUT_MODE(SP_X_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_build_event_kind_to_str(spn_build_event_kind_t kind) {
  return sp_str_view(event_info[kind].name);
}

spn_verbosity_t spn_build_event_get_verbosity(spn_build_event_kind_t kind) {
  return event_info[kind].verbosity;
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

static void spn_tui_write_transcript(sp_io_writer_t* w, sp_str_t lead, sp_str_t out, sp_str_t err) {
  sp_str_t parts[] = { lead, out, err };
  bool first = true;
  sp_carr_for(parts, it) {
    if (!sp_str_valid(parts[it])) continue;

    sp_str_t part = sp_str_trim_right(parts[it]);
    if (sp_str_empty(part)) continue;

    if (!first) {
      sp_io_write_new_line(w);
    }
    sp_io_write_str(w, part, SP_NULLPTR);
    first = false;
  }
}

static sp_str_t spn_tui_render_event_detail(sp_mem_t mem, spn_build_event_t* event) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &w);

  switch (event->kind) {
    case SPN_EVENT_SYNC: {
      sp_fmt_io(&w.base, "{} {.gray}", SP_FMT_STR(event->sync.name), SP_FMT_STR(event->sync.url));
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
        SP_FMT_STR(event->sync_pkg.name),
        SP_FMT_CSTR(event->sync_pkg.fetched ? "true" : "false"),
        SP_FMT_STR(event->sync_pkg.source_path)
      );
      break;
    }
    case SPN_EVENT_SYNC_END: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->sync_end.time);
      sp_fmt_io(&w.base, "synced {} packages in {.cyan}",
        SP_FMT_U32(event->sync_end.num_synced),
        SP_FMT_CSTR(buffer)
      );
      break;
    }
    case SPN_EVENT_COMPILE_START: {
      if (event->pkg) {
        sp_fmt_io(&w.base, "v{}", SP_FMT_STR(spn_semver_to_str(mem, event->pkg->version)));
      }
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_COMPILE: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->script_compile.time);
      sp_fmt_io(&w.base, "{.gray} in {.cyan}",
        SP_FMT_STR(event->script_compile.script_path),
        SP_FMT_CSTR(buffer)
      );
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_USER_FN: {
      sp_fmt_io(&w.base, "{}", SP_FMT_STR(event->node.info->tag));
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->package_ok.time);
      sp_fmt_io(&w.base, "in {.cyan}", SP_FMT_CSTR(buffer));
      break;
    }
    case SPN_EVENT_TARGET_BUILD_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->target.passed.time);
      sp_fmt_io(&w.base, "{.gray} in {.cyan}",
        SP_FMT_STR(event->target.passed.source_file),
        SP_FMT_CSTR(buffer)
      );
      break;
    }
    case SPN_EVENT_LINK_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->target.link_passed.time);
      sp_fmt_io(&w.base, "{.gray} in {.cyan}",
        SP_FMT_STR(event->target.link_passed.output_path),
        SP_FMT_CSTR(buffer)
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
      sp_fmt_io(&w.base, "{.gray} in {.cyan}",
        SP_FMT_STR(event->embed_passed.object_path),
        SP_FMT_CSTR(buffer)
      );
      break;
    }
    case SPN_EVENT_DIRTY_SUMMARY: {
      sp_fmt_io(&w.base, "commands={}/{} files={}/{} forced={}",
        SP_FMT_U32(event->dirty_summary.dirty_commands),
        SP_FMT_U32(event->dirty_summary.total_commands),
        SP_FMT_U32(event->dirty_summary.dirty_files),
        SP_FMT_U32(event->dirty_summary.total_files),
        SP_FMT_CSTR(event->dirty_summary.forced ? "true" : "false")
      );
      break;
    }
    case SPN_EVENT_BUILD_SUMMARY: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->build_summary.time);
      sp_fmt_io(&w.base, "{}/{} commands in {.cyan}",
        SP_FMT_U32(event->build_summary.num_dirty),
        SP_FMT_U32(event->build_summary.total_commands),
        SP_FMT_CSTR(buffer)
      );
      break;
    }
    case SPN_EVENT_RESOLVE_PACKAGE: {
      sp_fmt_io(&w.base, "{} {.cyan}",
        SP_FMT_STR(event->resolve_pkg.name),
        SP_FMT_STR(event->resolve_pkg.version)
      );
      break;
    }
    case SPN_EVENT_RESOLVE_END: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->resolve_end.time);
      sp_fmt_io(&w.base, "{} packages in {.cyan}",
        SP_FMT_U32(event->resolve_end.num_resolved),
        SP_FMT_CSTR(buffer)
      );
      break;
    }
    case SPN_EVENT_TARGET_RUN: {
      sp_fmt_io(&w.base, "{.gray}", SP_FMT_STR(event->run.command));
      break;
    }
    case SPN_EVENT_BUILD_PASSED: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->build.passed.time);
      sp_fmt_io(&w.base,
        "profile {.cyan} in {.cyan}",
        SP_FMT_STR(event->build.passed.profile->name),
        SP_FMT_CSTR(buffer)
      );
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_CRASHED: {
      sp_io_write_str(&w.base, sp_str_lit("build script crashed"), SP_NULLPTR);
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED: {
      sp_str_t lead = sp_fmt(mem, "{.cyan} failed to compile", SP_FMT_STR(event->compile_failed.script_path)).value;
      spn_tui_write_transcript(&w.base, lead, event->compile_failed.error, sp_str_lit(""));
      break;
    }
    case SPN_EVENT_TARGET_BUILD_FAILED: {
      sp_str_t lead = sp_fmt(mem, "{.cyan} failed to compile", SP_FMT_STR(event->target.failed.source_file)).value;
      spn_tui_write_transcript(&w.base, lead, event->target.failed.out, event->target.failed.err);
      break;
    }
    case SPN_EVENT_ERR_UNKNOWN_PKG: {
      sp_fmt_io(
        &w.base,
        "{.cyan} could not be located",
        SP_FMT_STR(event->unknown.request.qualified)
      );
      break;
    }
    case SPN_EVENT_ERR_UNSATISFIABLE_VERSION: {
      sp_fmt_io(
        &w.base,
        "no version of {.cyan} satisfies {.yellow}",
        SP_FMT_STR(event->unsatisfiable.low.qualified),
        SP_FMT_STR(spn_semver_range_to_str(mem, event->unsatisfiable.low.index.range))
      );
      break;
    }
    case SPN_EVENT_ERR_CIRCULAR_DEP: {
      sp_fmt_io(
        &w.base,
        "{.cyan} transitively includes itself",
        SP_FMT_STR(event->circular.id.name)
      );
      break;
    }
    case SPN_EVENT_ERR_MANIFEST: {
      sp_fmt_io(
        &w.base,
        "{.cyan} has an invalid manifest ({.gray}): {}",
        SP_FMT_STR(event->manifest_err.name),
        SP_FMT_STR(event->manifest_err.path),
        SP_FMT_STR(event->manifest_err.error)
      );
      break;
    }
    case SPN_EVENT_SYNC_FAILED: {
      sp_fmt_io(
        &w.base,
        "{.cyan} failed to sync from {.gray}: {}",
        SP_FMT_STR(event->sync_failed.name),
        SP_FMT_STR(event->sync_failed.url),
        SP_FMT_STR(event->sync_failed.error)
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
            SP_FMT_STR(event->err.manifest_parse.path)
          );
          break;
        }
        case SPN_ERR_MANIFEST_FIELD: {
          sp_fmt_io(
            &w.base,
            "invalid manifest field {.cyan}: expected {.yellow}, got {.red}",
            SP_FMT_STR(event->err.manifest_field.path),
            SP_FMT_STR(event->err.manifest_field.expected),
            SP_FMT_STR(event->err.manifest_field.actual)
          );
          break;
        }
        case SPN_ERR_MANIFEST_ISSUES: {
          sp_io_write_str(&w.base, sp_str_lit("invalid manifest:"), SP_NULLPTR);
          sp_da_for(event->err.issues, it) {
            sp_fmt_io(
              &w.base,
              "\n  - {}",
              SP_FMT_STR(spn_codegen_issue_message(mem, &event->err.issues[it]))
            );
          }
          break;
        }
        case SPN_ERR_TOOLCHAIN_FETCH: {
          sp_fmt_io(
            &w.base,
            "toolchain {.cyan} failed to download from {.gray}",
            SP_FMT_STR(event->err.artifact.name),
            SP_FMT_STR(event->err.artifact.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_NO_SHA: {
          sp_fmt_io(
            &w.base,
            "toolchain {.cyan} has no sha256 for {.gray}",
            SP_FMT_STR(event->err.artifact.name),
            SP_FMT_STR(event->err.artifact.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_SHA: {
          sp_fmt_io(
            &w.base,
            "toolchain {.cyan} sha256 mismatch for {.gray}: expected {.yellow}, got {.red}",
            SP_FMT_STR(event->err.artifact.name),
            SP_FMT_STR(event->err.artifact.url),
            SP_FMT_STR(event->err.artifact.expected),
            SP_FMT_STR(event->err.artifact.actual)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_EXTRACT: {
          sp_fmt_io(
            &w.base,
            "toolchain {.cyan} failed to extract archive from {.gray}",
            SP_FMT_STR(event->err.artifact.name),
            SP_FMT_STR(event->err.artifact.url)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_UNKNOWN: {
          sp_fmt_io(
            &w.base,
            "toolchain {.cyan} isn't defined",
            SP_FMT_STR(event->err.toolchain.name)
          );
          break;
        }
        case SPN_ERR_TOOLCHAIN_TARGET: {
          sp_str_t target = spn_triple_to_str(mem, event->err.toolchain.target);
          switch (event->err.toolchain.role) {
            case SPN_TOOLCHAIN_ROLE_BUILD: {
              sp_fmt_io(
                &w.base,
                "toolchain {.cyan} can't target {.yellow}",
                SP_FMT_STR(event->err.toolchain.name),
                SP_FMT_STR(target)
              );
              break;
            }
            case SPN_TOOLCHAIN_ROLE_SCRIPT: {
              sp_fmt_io(
                &w.base,
                "build scripts compile to {.yellow}, but toolchain {.cyan} can't target it",
                SP_FMT_STR(target),
                SP_FMT_STR(event->err.toolchain.name)
              );
              break;
            }
          }
          bool first = true;
          sp_str_ht_for_kv(event->err.toolchain.catalog->entries, it) {
            spn_toolchain_t* toolchain = *it.val;
            if (!spn_toolchain_supports(toolchain, event->err.toolchain.target, event->err.toolchain.host)) continue;
            sp_io_write_str(&w.base, first ? sp_str_lit("\ntoolchains that can: ") : sp_str_lit(", "), SP_NULLPTR);
            sp_fmt_io(&w.base, "{.green}", SP_FMT_STR(toolchain->name));
            first = false;
          }
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
        SP_FMT_STR(event->api_call.fn),
        SP_FMT_STR(event->api_call.args)
      );
      break;
    }
    case SPN_EVENT_INIT_BUILD_GRAPH: {
      sp_fmt_io(&w.base, "profile={} force={} packages={}",
        SP_FMT_STR(event->graph_init.profile),
        SP_FMT_CSTR(event->graph_init.force ? "true" : "false"),
        SP_FMT_U32(event->graph_init.packages)
      );
      break;
    }
    case SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED: {
      switch (event->err.build_graph.kind) {
        case SPN_BUILD_GRAPH_ERR_MISSING_INPUT: {
          sp_fmt_io(
            &w.base,
            "missing build graph input {.cyan}",
            SP_FMT_STR(event->err.build_graph.file)
          );
          break;
        }
        case SPN_BUILD_GRAPH_ERR_DUPLICATE_OUTPUT: {
          sp_fmt_io(
            &w.base,
            "two graph nodes output the same file {.cyan}",
            SP_FMT_STR(event->err.build_graph.file)
          );
          if (sp_str_valid(event->err.build_graph.command_a)) {
            sp_io_write_c8(&w.base, '\n');
            sp_io_write_str(&w.base, event->err.build_graph.command_a, SP_NULLPTR);
          }
          if (sp_str_valid(event->err.build_graph.command_b)) {
            sp_io_write_c8(&w.base, '\n');
            sp_io_write_str(&w.base, event->err.build_graph.command_b, SP_NULLPTR);
          }
          break;
        }
        case SPN_BUILD_GRAPH_ERR_UNKNOWN: {
          sp_io_write_str(&w.base, sp_str_lit("failed to prepare build graph"), SP_NULLPTR);
          break;
        }
      }
      break;
    }
    case SPN_EVENT_LINK_START: {
      sp_fmt_io(&w.base, "target={} objects={} output={}",
        SP_FMT_STR(event->target.name),
        SP_FMT_U32(event->target.link_start.num_objects),
        SP_FMT_STR(event->target.link_start.output_path)
      );
      break;
    }
    case SPN_EVENT_LINK_FAILED: {
      sp_str_t lead = sp_fmt(mem, "{.cyan} failed to link", SP_FMT_STR(event->target.name)).value;
      spn_tui_write_transcript(&w.base, lead, event->target.link_failed.err, event->target.link_failed.out);
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK: {
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->configure.time);
      sp_fmt_io(&w.base, "in {.cyan}", SP_FMT_CSTR(buffer));
      break;
    }
    case SPN_EVENT_EMBED_FAILED: {
      sp_fmt_io(&w.base, "{}: {}",
        SP_FMT_STR(event->embed_failed.path),
        SP_FMT_STR(event->embed_failed.error)
      );
      break;
    }
    case SPN_EVENT_BUILD_FAILED: {
      sp_fmt_io(&w.base, "profile {.cyan} failed with {} {}",
        SP_FMT_STR(event->build_failed.profile),
        SP_FMT_U32(event->build_failed.num_errors),
        SP_FMT_CSTR(event->build_failed.num_errors == 1 ? "error" : "errors")
      );
      if (!sp_str_empty(event->build_failed.first_error)) {
        sp_fmt_io(&w.base, " ({})", SP_FMT_STR(event->build_failed.first_error));
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

static sp_str_t spn_tui_render_coarse_line(sp_mem_t mem, sp_str_t verb, sp_str_t pkg_name, sp_str_t detail) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &w);

  sp_fmt_io(&w.base, "{:>12 .bold .green}", SP_FMT_STR(verb));
  if (!sp_str_empty(pkg_name)) {
    sp_fmt_io(&w.base, " {}", SP_FMT_STR(spn_tui_decorate_name(mem, pkg_name, 0, ' ')));
  }
  if (!sp_str_empty(detail)) {
    sp_fmt_io(&w.base, " {}", SP_FMT_STR(detail));
  }

  return sp_io_dyn_mem_writer_take_str(&w);
}

static sp_str_t spn_tui_render_coarse_error(sp_mem_t mem, spn_build_event_t* event) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &w);

  sp_fmt_io(&w.base, "{:>12 .bold .red}", SP_FMT_CSTR("error"));
  if (event->pkg) {
    sp_fmt_io(&w.base, " {}", SP_FMT_STR(spn_tui_decorate_name(mem, event->pkg->name, 0, ' ')));
  }
  sp_fmt_io(&w.base, " {}", SP_FMT_STR(spn_tui_render_event_detail(mem, event)));

  return sp_io_dyn_mem_writer_take_str(&w);
}

typedef struct {
  sp_str_t pkg;
  sp_str_t message;
} spn_tui_buffered_log_t;

sp_str_t spn_tui_render_coarse_event(sp_mem_t mem, spn_build_event_t* event) {
  static sp_str_ht(bool) seen_url = SP_NULLPTR;
  static sp_da(spn_tui_buffered_log_t) buffered_logs = SP_NULLPTR;
  static u32 num_downloads = 0;
  if (!seen_url) {
    sp_str_ht_init(spn.heap, seen_url);
    sp_da_init(spn.heap, buffered_logs);
  }

  spn_build_event_display_t display = event_info[event->kind];
  bool visible = display.verbosity <= spn.logger.verbosity;

  if (!visible) {
    if (event->kind == SPN_EVENT_USER_LOG) {
      sp_da_push(buffered_logs, ((spn_tui_buffered_log_t) {
        .pkg = event->pkg ? sp_str_copy(spn.heap, event->pkg->name) : sp_str_lit(""),
        .message = sp_str_copy(spn.heap, event->user_log.message),
      }));
    }
    return sp_str_lit("");
  }

  if (display.error) {
    sp_io_dyn_mem_writer_t w = sp_zero;
    sp_io_dyn_mem_writer_init(mem, &w);
    sp_da_for(buffered_logs, it) {
      spn_tui_buffered_log_t* log = &buffered_logs[it];
      sp_io_write_str(&w.base, spn_tui_render_coarse_line(mem, sp_str_lit(""), log->pkg, log->message), SP_NULLPTR);
      sp_io_write_c8(&w.base, '\n');
    }
    sp_da_clear(buffered_logs);
    sp_io_write_str(&w.base, spn_tui_render_coarse_error(mem, event), SP_NULLPTR);
    return sp_io_dyn_mem_writer_take_str(&w);
  }

  switch (event->kind) {
    case SPN_EVENT_SYNC: {
      if (sp_str_ht_get(seen_url, event->sync.url)) {
        return sp_str_lit("");
      }
      sp_str_ht_insert(seen_url, sp_str_copy(spn.heap, event->sync.url), true);
      num_downloads++;
      return spn_tui_render_coarse_line(
        mem,
        sp_str_lit("Downloading"),
        spn_tui_short_name(event->sync.name),
        sp_fmt(mem, "{.gray}", SP_FMT_STR(event->sync.url)).value
      );
    }

    case SPN_EVENT_SYNC_END: {
      if (!num_downloads) {
        return sp_str_lit("");
      }
      c8 buffer [64] = sp_zero;
      sp_fmt_write_duration_buf(buffer, sizeof(buffer), event->sync_end.time);
      return spn_tui_render_coarse_line(
        mem,
        sp_str_lit("Downloaded"),
        sp_str_lit(""),
        sp_fmt(mem, "{} {} in {.cyan}",
          SP_FMT_U32(num_downloads),
          SP_FMT_CSTR(num_downloads == 1 ? "package" : "packages"),
          SP_FMT_CSTR(buffer)
        ).value
      );
    }

    case SPN_EVENT_RESOLVE_END:
    case SPN_EVENT_BUILD_PASSED: {
      return spn_tui_render_coarse_line(
        mem,
        sp_str_view(display.name),
        sp_str_lit(""),
        spn_tui_render_event_detail(mem, event)
      );
    }

    case SPN_EVENT_TARGET_RUN: {
      sp_str_t name = event->run.name;
      if (sp_str_empty(name) && event->pkg) {
        name = event->pkg->name;
      }
      return spn_tui_render_coarse_line(
        mem,
        sp_str_view(display.name),
        name,
        spn_tui_render_event_detail(mem, event)
      );
    }

    case SPN_EVENT_USER_LOG: {
      sp_str_t name = event->pkg ? event->pkg->name : sp_str_lit("");
      return spn_tui_render_coarse_line(mem, sp_str_lit(""), name, event->user_log.message);
    }

    default: {
      sp_str_t name = event->pkg ? event->pkg->name : sp_str_lit("");
      return spn_tui_render_coarse_line(
        mem,
        sp_str_view(display.name),
        name,
        spn_tui_render_event_detail(mem, event)
      );
    }
  }
}

void spn_tui_init(spn_tui_t* tui, spn_tui_mode_t mode) {
  tui->mode = mode;

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

