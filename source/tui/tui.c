#include "profile/types.h"

#include "ctx/ctx.h"
#include "log/log.h"
#include "semver/convert.h"
#include "sp/color.h"
#include "sp/macro.h"
#include "sp/str.h"
#include "tui/tui.h"
#include "tui/spinner.h"

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

typedef enum {
  WHITE,
  GREEN,
  RED,
} spn_build_event_color_t;

#define BOLD     true
#define NOT_BOLD false

typedef struct {
  const c8* name;
  const c8* id;
  spn_build_event_color_t color;
  spn_verbosity_t verbosity;
  bool bold;
} spn_build_event_display_t;

#define EVENT(ID, NAME, COLOR, VERBOSITY, BOLD) [ID] = { NAME, sp_mstr(ID), COLOR, VERBOSITY, BOLD }

static spn_build_event_display_t event_info[] = {
  EVENT(SPN_EVENT_FETCH,                         "fetch", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_ERR,                           "failed", RED,   SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_ERR_CIRCULAR_DEP,              "failed", RED,   SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_ERR_UNKNOWN_PKG,               "failed", RED,   SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_RESOLVE,                       "resolve", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_SYNC,                          "sync", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_CHECKOUT,                      "checkout", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_SCRIPT_COMPILE,          "compile", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED,   "failed", RED,   SPN_VERBOSITY_QUIET,  NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_SCRIPT_CONFIGURE,        "configure", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_SCRIPT_BUILD,            "build", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_SCRIPT_PACKAGE,          "package", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_SCRIPT_FAILED,           "failed", RED,   SPN_VERBOSITY_QUIET,  NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_SCRIPT_CRASHED,          "failed", RED,   SPN_VERBOSITY_QUIET,  NOT_BOLD),
  EVENT(SPN_EVENT_DEP_BUILD,                     "dep:build", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_DEP_BUILD_PASSED,              "ok", GREEN, SPN_VERBOSITY_NORMAL, BOLD    ),
  EVENT(SPN_EVENT_DEP_BUILD_FAILED,              "failed", RED,   SPN_VERBOSITY_QUIET,  NOT_BOLD),
  EVENT(SPN_EVENT_TARGET_BUILD, "compile", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_TARGET_BUILD_PASSED, "ok", GREEN, SPN_VERBOSITY_NORMAL, BOLD    ),
  EVENT(SPN_EVENT_TARGET_BUILD_FAILED, "failed", RED,   SPN_VERBOSITY_QUIET,  NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_PASSED, "built", GREEN, SPN_VERBOSITY_NORMAL, BOLD    ),
  EVENT(SPN_EVENT_TCC_ERROR, "error", RED,   SPN_VERBOSITY_QUIET,  NOT_BOLD),
  EVENT(SPN_EVENT_TEST_RUN, "run", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_TEST_PASSED, "ok", GREEN, SPN_VERBOSITY_NORMAL, BOLD    ),
  EVENT(SPN_EVENT_TESTS_PASSED, "tested", GREEN, SPN_VERBOSITY_NORMAL, BOLD    ),
  EVENT(SPN_EVENT_TEST_FAILED, "failed", RED,   SPN_VERBOSITY_QUIET,  NOT_BOLD),
  EVENT(SPN_EVENT_CLEAN, "clean", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_GENERATE, "generate", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_SCRIPT_USER_FN, "fn", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_ADD_TARGET, "add_unit", WHITE,  SPN_VERBOSITY_DEBUG,  NOT_BOLD),
  EVENT(SPN_EVENT_DEBUG, "debug", WHITE,  SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_ADD_SOURCE, "debug", WHITE,  SPN_VERBOSITY_DEBUG,  NOT_BOLD),
  EVENT(SPN_EVENT_INIT_BUILD_GRAPH, "debug", WHITE,  SPN_VERBOSITY_DEBUG,  NOT_BOLD),
  EVENT(SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED, "failed", RED, SPN_VERBOSITY_QUIET, NOT_BOLD),
  EVENT(SPN_EVENT_LINK_START,  "link",   WHITE, SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_LINK_PASSED, "ok",     GREEN, SPN_VERBOSITY_NORMAL, BOLD),
  EVENT(SPN_EVENT_LINK_FAILED, "failed", RED,   SPN_VERBOSITY_QUIET,  NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK, "debug", WHITE,  SPN_VERBOSITY_DEBUG,  NOT_BOLD),
  EVENT(SPN_EVENT_CLI_ENTRY,    "entry", WHITE,  SPN_VERBOSITY_DEBUG,  NOT_BOLD),
  EVENT(SPN_EVENT_RESOLVE_START,   "resolve", WHITE, SPN_VERBOSITY_DEBUG, NOT_BOLD),
  EVENT(SPN_EVENT_RESOLVE_PACKAGE, "resolve", WHITE, SPN_VERBOSITY_DEBUG, NOT_BOLD),
  EVENT(SPN_EVENT_RESOLVE_END,     "resolve", WHITE, SPN_VERBOSITY_DEBUG, NOT_BOLD),
  EVENT(SPN_EVENT_SYNC_START,      "sync",    WHITE, SPN_VERBOSITY_DEBUG, NOT_BOLD),
  EVENT(SPN_EVENT_SYNC_PACKAGE,    "sync",    WHITE, SPN_VERBOSITY_DEBUG, NOT_BOLD),
  EVENT(SPN_EVENT_SYNC_FAILED,     "sync",    RED,   SPN_VERBOSITY_DEBUG, NOT_BOLD),
  EVENT(SPN_EVENT_SYNC_END,        "sync",    WHITE, SPN_VERBOSITY_DEBUG, NOT_BOLD),
  EVENT(SPN_EVENT_API_CALL,        "api",     WHITE, SPN_VERBOSITY_DEBUG, NOT_BOLD),
  EVENT(SPN_EVENT_USER_LOG,        "log",     WHITE, SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_EMBED_START,     "embed",   WHITE, SPN_VERBOSITY_NORMAL, NOT_BOLD),
  EVENT(SPN_EVENT_EMBED_PASSED,    "ok",      GREEN, SPN_VERBOSITY_NORMAL, BOLD),
  EVENT(SPN_EVENT_EMBED_FAILED,    "failed",  RED,   SPN_VERBOSITY_QUIET,  NOT_BOLD),
  EVENT(SPN_EVENT_DIRTY_SUMMARY,   "dirty",   WHITE, SPN_VERBOSITY_DEBUG,  NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_FAILED,    "failed",  RED,   SPN_VERBOSITY_QUIET,  NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_SUMMARY,   "summary", WHITE, SPN_VERBOSITY_DEBUG,  NOT_BOLD),
  EVENT(SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK, "debug", WHITE, SPN_VERBOSITY_DEBUG, NOT_BOLD),
};

static sp_str_t spn_tui_name_to_color(sp_str_t str);
static sp_str_t spn_tui_decorate_name(sp_str_t name, u32 padded_len, c8 pad);

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
  return sp_str_from_cstr(spn_mem_todo, event_info[kind].name);
}

spn_verbosity_t spn_build_event_get_verbosity(spn_build_event_kind_t kind) {
  return event_info[kind].verbosity;
}

void sp_tui_print(sp_str_t str) {
  sp_io_write_str(spn_ctx_get_log_err(), str, SP_NULLPTR);
}

void sp_tui_up(u32 n) {
  sp_str_t command = sp_format("\033[{}A", SP_FMT_U32(n));
  sp_tui_print(command);
}

void sp_tui_down(u32 n) {
  sp_str_t command = sp_format("\033[{}B", SP_FMT_U32(n));
  sp_tui_print(command);
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

static sp_str_t spn_tui_color_name(sp_str_t name) {
  return sp_format("{}{}{}",
    SP_FMT_STR(spn_tui_name_to_color(name)),
    SP_FMT_STR(name),
    SP_FMT_CSTR(SP_ANSI_RESET)
  );
}

static sp_str_t spn_tui_decorate_name(sp_str_t name, u32 padded_len, c8 pad) {
  sp_str_builder_t b = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&b, spn_tui_name_to_color(name));
  sp_str_builder_append_cstr(&b, "\u2590 ");
  sp_str_builder_append(&b, name);
  sp_str_builder_append_cstr(&b, SP_ANSI_RESET);

  if (padded_len > name.len) {
    sp_str_builder_append(&b, sp_str_repeat(pad, padded_len - name.len));
  }

  return sp_str_builder_to_str(&b);
}

static sp_str_t spn_tui_name_to_color(sp_str_t str) {
  if (sp_str_equal_cstr(str, "package")) {
    return sp_str_lit(SP_ANSI_FG_WHITE);
  }

  static sp_ht(u32, sp_hash_t) buckets = SP_NULLPTR;

  sp_hash_t hash = sp_hash_cstr(sp_str_to_cstr(spn_mem_todo, str));
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

  return sp_color_to_tui_rgb_f(r, g, b);
}

sp_str_t spn_tui_render_event(spn_build_event_t* event, u32 max_name) {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  spn_build_event_display_t display = event_info[event->kind];
  sp_str_t name = spn_build_event_kind_to_str(event->kind);
  if (display.bold) {
    sp_str_builder_append_cstr(&builder, SP_ANSI_BOLD);
  }
  switch (display.color) {
    case WHITE: {
      sp_str_builder_append_fmt(&builder, "{:<9 .black}", SP_FMT_STR(name));
      break;
    }
    case GREEN: {
      sp_str_builder_append_fmt(&builder, "{:<9 .fg green}", SP_FMT_STR(name));
      break;
    }
    case RED: {
      sp_str_builder_append_fmt(&builder, "{:<9 .fg red}", SP_FMT_STR(name));
      break;
    }
  }
  if (display.bold) {
    sp_str_builder_append_cstr(&builder, SP_ANSI_RESET);
  }
  sp_str_builder_append_c8(&builder, ' ');

  sp_str_t package = strl("root");
  if (event->pkg) {
    package = event->pkg->name;
  }
  sp_str_builder_append(&builder, spn_tui_decorate_name(package, max_name, ' '));
  sp_str_builder_append_c8(&builder, ' ');

  switch (event->kind) {
    case SPN_EVENT_SYNC: {
      sp_str_builder_append_fmt(&builder, "{.black} ", SP_FMT_STR(event->sync.url));
      break;
    }
    case SPN_EVENT_CHECKOUT: {
      sp_str_builder_append_fmt(&builder,
        "{} {.black} {}{}{}",
        SP_FMT_STR(spn_semver_to_str(event->checkout.version)),
        SP_FMT_STR(sp_str_truncate(spn_mem_todo, event->checkout.commit, 8, SP_ZERO_STRUCT(sp_str_t))),
        SP_FMT_CSTR(SP_ANSI_ITALIC),
        SP_FMT_STR(sp_str_truncate(spn_mem_todo, event->checkout.message, 32, sp_str_lit("..."))),
        SP_FMT_CSTR(SP_ANSI_RESET)
      );
      break;
    }
    case SPN_EVENT_RESOLVE: {
      switch (event->resolve.strategy) {
        case SPN_RESOLVE_STRATEGY_SOLVER: {
          sp_str_builder_append_fmt(&builder, "{.black}", SP_FMT_CSTR("using solver"));
          break;
        }
        case SPN_RESOLVE_STRATEGY_LOCK_FILE: {
          sp_str_builder_append_fmt(&builder, "{.black}", SP_FMT_CSTR("using lockfile"));
          break;
        }
      }
      break;
    }
    case SPN_EVENT_TESTS_PASSED: {
      sp_str_builder_append_fmt(
        &builder,
        "Ran {} tests for profile {.cyan} in {.cyan}s",
        SP_FMT_U32(event->test.passed.n),
        SP_FMT_STR(event->test.passed.profile->name),
        SP_FMT_F32(sp_tm_ns_to_s_f(event->test.passed.time))
      );
      break;
    }
    case SPN_EVENT_TEST_FAILED: {
      sp_str_builder_append_fmt(&builder, "returned code {}", SP_FMT_S32(1));
      break;
    }
    case SPN_EVENT_DEP_BUILD_PASSED: {
      sp_str_builder_append_fmt(&builder,
        "built in {.cyan}s",
        SP_FMT_F32(sp_tm_ns_to_s_f(event->dep.passed.time))
      );
      break;
    }
    case SPN_EVENT_BUILD_PASSED: {
      sp_str_builder_append_fmt(&builder,
        "Built profile {.cyan} in {.cyan}s",
        SP_FMT_STR(event->build.passed.profile->name),
        SP_FMT_F32(sp_tm_ns_to_s_f(event->build.passed.time))
      );
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_CRASHED: {
      sp_str_builder_append_cstr(&builder, "crashed");
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED: {
      sp_str_builder_append(&builder, event->compile_failed.error);
      break;
    }
    case SPN_EVENT_TARGET_BUILD_FAILED: {
      sp_str_builder_append(&builder, event->target.failed.out);
      sp_str_builder_new_line(&builder);
      sp_str_builder_append(&builder, event->target.failed.err);
      break;
    }
    case SPN_EVENT_ERR_UNKNOWN_PKG: {
      sp_str_builder_append_fmt(
        &builder,
        "{.cyan} could not be located",
        SP_FMT_STR(event->unknown.request.qualified)
      );
      break;
    }
    case SPN_EVENT_ERR_CIRCULAR_DEP: {
      sp_str_builder_append_fmt(
        &builder,
        "{.cyan} transitively includes itself",
        SP_FMT_STR(event->circular.id.name)
      );
      break;
    }
    case SPN_EVENT_ERR: {
      switch (event->err.kind) {
        // case SPN_ERR_OK: {
        //   break;
        // }
        case SPN_ERR_MANIFEST_PARSE: {
          sp_str_builder_append_fmt(
            &builder,
            "failed to parse manifest {.cyan}",
            SP_FMT_STR(event->err.manifest_parse.path)
          );
          break;
        }
        case SPN_ERR_MANIFEST_FIELD: {
          sp_str_builder_append_fmt(
            &builder,
            "invalid manifest field {.cyan}: expected {.yellow}, got {.red}",
            SP_FMT_STR(event->err.manifest_field.path),
            SP_FMT_STR(event->err.manifest_field.expected),
            SP_FMT_STR(event->err.manifest_field.actual)
          );
          break;
        }
        default: {
          sp_str_builder_append_cstr(&builder, "unknown error");
          break;
        }
      }
      break;
    }
    case SPN_EVENT_CLEAN: {
      sp_str_builder_append_fmt(&builder, "{.cyan}", SP_FMT_STR(event->clean.path));
      break;
    }
    case SPN_EVENT_GENERATE: {
      sp_str_builder_append_fmt(&builder, "{.cyan}", SP_FMT_STR(event->generate.path));
      break;
    }
    case SPN_EVENT_DEBUG: {
      sp_str_builder_append(&builder, event->debug.message);
      break;
    }
    case SPN_EVENT_USER_LOG: {
      sp_str_builder_append(&builder, event->user_log.message);
      break;
    }
    case SPN_EVENT_ADD_TARGET: {
      sp_str_builder_append_fmt(&builder, "target={} kind={}",
        SP_FMT_STR(event->target.name),
        SP_FMT_S32(event->target.add_debug.kind)
      );
      break;
    }
    case SPN_EVENT_ADD_SOURCE: {
      sp_str_builder_append_fmt(&builder, "target={} source={}",
        SP_FMT_STR(event->target.name),
        SP_FMT_STR(event->target.source.source)
      );
      break;
    }
    case SPN_EVENT_INIT_BUILD_GRAPH: {
      sp_str_builder_append_fmt(&builder, "profile={} force={} packages={}",
        SP_FMT_STR(event->graph_init.profile),
        SP_FMT_CSTR(event->graph_init.force ? "true" : "false"),
        SP_FMT_U32(event->graph_init.packages)
      );
      break;
    }
    case SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED: {
      switch (event->err.build_graph.kind) {
        case SPN_BUILD_GRAPH_ERR_MISSING_INPUT: {
          sp_str_builder_append_fmt(
            &builder,
            "missing build graph input {.cyan}",
            SP_FMT_STR(event->err.build_graph.file)
          );
          break;
        }
        case SPN_BUILD_GRAPH_ERR_DUPLICATE_OUTPUT: {
          sp_str_builder_append_fmt(
            &builder,
            "two graph nodes output the same file {.cyan}",
            SP_FMT_STR(event->err.build_graph.file)
          );
          if (sp_str_valid(event->err.build_graph.command_a)) {
            sp_str_builder_new_line(&builder);
            sp_str_builder_append(&builder, event->err.build_graph.command_a);
          }
          if (sp_str_valid(event->err.build_graph.command_b)) {
            sp_str_builder_new_line(&builder);
            sp_str_builder_append(&builder, event->err.build_graph.command_b);
          }
          break;
        }
        case SPN_BUILD_GRAPH_ERR_UNKNOWN: {
          sp_str_builder_append_cstr(&builder, "failed to prepare build graph");
          break;
        }
      }
      break;
    }
    case SPN_EVENT_LINK_START: {
      sp_str_builder_append_fmt(&builder, "target={} objects={} output={}",
        SP_FMT_STR(event->target.name),
        SP_FMT_U32(event->target.link_start.num_objects),
        SP_FMT_STR(event->target.link_start.output_path)
      );
      break;
    }
    case SPN_EVENT_LINK_FAILED: {
      if (sp_str_valid(event->target.link_failed.err)) {
        sp_str_builder_append(&builder, event->target.link_failed.err);
      }
      if (sp_str_valid(event->target.link_failed.out)) {
        sp_str_builder_append(&builder, event->target.link_failed.out);
      }
      break;
    }
    case SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK: {
      sp_str_builder_append_fmt(&builder, "time_ns={}",
        SP_FMT_U64(event->configure.time)
      );
      break;
    }
    case SPN_EVENT_TARGET_BUILD: {
      break;
    }
    case SPN_EVENT_EMBED_FAILED: {
      sp_str_builder_append_fmt(&builder, "{}: {}",
        SP_FMT_STR(event->embed_failed.path),
        SP_FMT_STR(event->embed_failed.error)
      );
      break;
    }
    case SPN_EVENT_BUILD_FAILED: {
      sp_str_builder_append(&builder, event->build_failed.first_error);
      break;
    }
    default: {
      break;
    }
  }

  return sp_str_builder_to_str(&builder);
}

void spn_tui_init(spn_tui_t* tui, spn_tui_mode_t mode) {
  tui->mode = mode;
  tui->info.max_name = 16;

  spn_spinner_init(&tui->spinner, sp_color_rgb_255(99, 160, 136));

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
