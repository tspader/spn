#include "tui.h"

#include "ctx.h"

#include "sp/color.h"
#include "sp/ht.h"
#include "sp/str.h"

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
  SPN_EVENT_COLOR_NONE,
  SPN_EVENT_COLOR_GREEN,
  SPN_EVENT_COLOR_RED,
} spn_build_event_color_t;

#define SPN_EVENT_BOLD     true
#define SPN_EVENT_NOT_BOLD false

typedef struct {
  const c8* name;
  spn_build_event_color_t color;
  spn_verbosity_t verbosity;
  bool bold;
} spn_build_event_display_t;

static spn_build_event_display_t event_info[] = {
  [SPN_EVENT_FETCH]                         = { "fetch",          SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_ERR_CIRCULAR_DEP]              = { "failed",         SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_ERR_UNKNOWN_PKG]               = { "failed",         SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_RESOLVE]                       = { "resolve",        SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_SYNC]                          = { "sync",           SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_CHECKOUT]                      = { "checkout",       SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_SCRIPT_COMPILE]          = { "compile",        SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED]   = { "failed",         SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_QUIET,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_SCRIPT_CONFIGURE]        = { "configure",      SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_SCRIPT_BUILD]            = { "build",          SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_SCRIPT_PACKAGE]          = { "package",        SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_SCRIPT_FAILED]           = { "failed",         SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_QUIET,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_SCRIPT_CRASHED]          = { "failed",         SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_QUIET,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_SCRIPT_CONFIGURE_FAILED] = { "failed",         SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_QUIET,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_SCRIPT_BUILD_FAILED]     = { "failed",         SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_QUIET,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_SCRIPT_PACKAGE_FAILED]   = { "failed",         SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_QUIET,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_DEP_BUILD]                     = { "dep::build",     SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_DEP_BUILD_PASSED]              = { "ok",             SPN_EVENT_COLOR_GREEN, SPN_VERBOSITY_NORMAL, SPN_EVENT_BOLD     },
  [SPN_EVENT_DEP_BUILD_FAILED]              = { "failed",         SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_QUIET,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_TARGET_BUILD]                  = { "compile",        SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_TARGET_BUILD_PASSED]           = { "ok",             SPN_EVENT_COLOR_GREEN, SPN_VERBOSITY_NORMAL, SPN_EVENT_BOLD     },
  [SPN_EVENT_TARGET_BUILD_FAILED]           = { "failed",         SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_QUIET,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_PASSED]                  = { "built",          SPN_EVENT_COLOR_GREEN, SPN_VERBOSITY_NORMAL, SPN_EVENT_BOLD     },
  [SPN_EVENT_TCC_ERROR]                     = { "error",          SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_QUIET,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_TEST_RUN]                      = { "run",            SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_TEST_PASSED]                   = { "ok",             SPN_EVENT_COLOR_GREEN, SPN_VERBOSITY_NORMAL, SPN_EVENT_BOLD     },
  [SPN_EVENT_TESTS_PASSED]                  = { "tested",         SPN_EVENT_COLOR_GREEN, SPN_VERBOSITY_NORMAL, SPN_EVENT_BOLD     },
  [SPN_EVENT_TEST_FAILED]                   = { "failed",         SPN_EVENT_COLOR_RED,   SPN_VERBOSITY_QUIET,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_CLEAN]                         = { "clean",          SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_GENERATE]                      = { "generate",       SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_BUILD_SCRIPT_USER_FN]          = { "fn",             SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_ADD_TARGET]                    = { "add_unit",       SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_DEBUG,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_DEBUG]                         = { "debug",          SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_NORMAL, SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_ADD_SOURCE]                    = { "debug",          SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_DEBUG,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_INIT_BUILD_GRAPH]              = { "debug",          SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_DEBUG,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_LINK_TARGET]                   = { "debug",          SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_DEBUG,  SPN_EVENT_NOT_BOLD },
  [SPN_EVENT_RUN_CONFIGURE]                 = { "debug",          SPN_EVENT_COLOR_NONE,  SPN_VERBOSITY_DEBUG,  SPN_EVENT_NOT_BOLD },
};

static sp_str_t spn_tui_name_to_color(sp_str_t str);
static sp_str_t spn_tui_decorate_name(sp_str_t name, u32 padded_len, c8 pad);
static u32 sp_str_visual_len(sp_str_t str);
static sp_str_t sp_str_visual_pad(sp_str_t str, u32 target_visual_width);
static void sp_tui_apply_indent(sp_str_builder_t* builder, u32 indent);

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

  SP_FATAL("Unknown output mode {:fg brightyellow}; options are [interactive, noninteractive, quiet, none]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_OUTPUT_MODE_NONE);
}

sp_str_t spn_output_mode_to_str(spn_tui_mode_t mode) {
  switch (mode) {
    SPN_OUTPUT_MODE(SP_X_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_build_event_kind_to_str(spn_build_event_kind_t kind) {
  return sp_str_from_cstr(event_info[kind].name);
}

spn_verbosity_t spn_build_event_get_verbosity(spn_build_event_kind_t kind) {
  return event_info[kind].verbosity;
}

void sp_tui_print(sp_str_t str) {
  sp_io_write_str(spn_ctx_get_log_err(), str);
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

  sp_hash_t hash = sp_hash_cstr(sp_str_to_cstr(str));
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

sp_str_t spn_tui_render_build_event(spn_build_event_t* event, u32 max_name) {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  spn_build_event_display_t display = event_info[event->kind];
  sp_str_t name = spn_build_event_kind_to_str(event->kind);
  if (display.bold) {
    sp_str_builder_append_cstr(&builder, SP_ANSI_BOLD);
  }
  switch (display.color) {
    case SPN_EVENT_COLOR_NONE: {
      sp_str_builder_append_fmt(&builder, "{:fg brightblack :pad 9}", SP_FMT_STR(name));
      break;
    }
    case SPN_EVENT_COLOR_GREEN: {
      sp_str_builder_append_fmt(&builder, "{:fg green :pad 9}", SP_FMT_STR(name));
      break;
    }
    case SPN_EVENT_COLOR_RED: {
      sp_str_builder_append_fmt(&builder, "{:fg red :pad 9}", SP_FMT_STR(name));
      break;
    }
  }
  if (display.bold) {
    sp_str_builder_append_cstr(&builder, SP_ANSI_RESET);
  }
  sp_str_builder_append_c8(&builder, ' ');

  sp_str_builder_append(&builder, spn_tui_decorate_name(event->pkg->name, max_name, ' '));
  sp_str_builder_append_c8(&builder, ' ');

  switch (event->kind) {
    case SPN_EVENT_SYNC: {
      sp_str_builder_append_fmt(&builder, "{:fg brightblack} ", SP_FMT_STR(event->sync.url));
      break;
    }
    case SPN_EVENT_CHECKOUT: {
      sp_str_builder_append_fmt(&builder,
        "{} {:fg brightblack} {}{}{}",
        SP_FMT_STR(spn_semver_to_str(event->checkout.version)),
        SP_FMT_STR(sp_str_truncate(event->checkout.commit, 8, SP_ZERO_STRUCT(sp_str_t))),
        SP_FMT_CSTR(SP_ANSI_ITALIC),
        SP_FMT_STR(sp_str_truncate(event->checkout.message, 32, sp_str_lit("..."))),
        SP_FMT_CSTR(SP_ANSI_RESET)
      );
      break;
    }
    case SPN_EVENT_RESOLVE: {
      switch (event->resolve.strategy) {
        case SPN_RESOLVE_STRATEGY_SOLVER: {
          sp_str_builder_append_fmt(&builder, "{:fg brightblack}", SP_FMT_CSTR("using solver"));
          break;
        }
        case SPN_RESOLVE_STRATEGY_LOCK_FILE: {
          sp_str_builder_append_fmt(&builder, "{:fg brightblack}", SP_FMT_CSTR("using lockfile"));
          break;
        }
      }
      break;
    }
    case SPN_EVENT_TESTS_PASSED: {
      sp_str_builder_append_fmt(
        &builder,
        "Ran {} tests for profile {:fg brightcyan} in {:fg brightcyan}s",
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
        "built in {:fg brightcyan}s",
        SP_FMT_F32(sp_tm_ns_to_s_f(event->dep.passed.time))
      );
      break;
    }
    case SPN_EVENT_BUILD_PASSED: {
      sp_str_builder_append_fmt(&builder,
        "Built profile {:fg brightcyan} in {:fg brightcyan}s",
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
        "{:fg brightcyan} could not be located",
        SP_FMT_STR(event->unknown.request.name)
      );
      break;
    }
    case SPN_EVENT_ERR_CIRCULAR_DEP: {
      sp_str_builder_append_fmt(
        &builder,
        "{:fg brightcyan} transitively includes itself",
        SP_FMT_STR(event->circular.pkg->name)
      );
      break;
    }
    case SPN_EVENT_CLEAN: {
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan}", SP_FMT_STR(event->clean.path));
      break;
    }
    case SPN_EVENT_GENERATE: {
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan}", SP_FMT_STR(event->generate.path));
      break;
    }
    case SPN_EVENT_DEBUG: {
      sp_str_builder_append(&builder, event->debug.message);
      break;
    }
    case SPN_EVENT_ADD_TARGET: {
      sp_str_builder_append_fmt(&builder, "target={} kind={}",
        SP_FMT_STR(event->target_add.target),
        SP_FMT_S32(event->target_add.kind)
      );
      break;
    }
    case SPN_EVENT_ADD_SOURCE: {
      sp_str_builder_append_fmt(&builder, "target={} source={}",
        SP_FMT_STR(event->target_source.target),
        SP_FMT_STR(event->target_source.source)
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
    case SPN_EVENT_LINK_TARGET: {
      sp_str_builder_append_fmt(&builder, "target={} objects={}",
        SP_FMT_STR(event->target_link.target),
        SP_FMT_U32(event->target_link.objects)
      );
      break;
    }
    case SPN_EVENT_RUN_CONFIGURE: {
      sp_str_builder_append_fmt(&builder, "has_hook={} result={} time_ns={}",
        SP_FMT_CSTR(event->configure.exists ? "true" : "false"),
        SP_FMT_S32(event->configure.result),
        SP_FMT_U64(event->configure.time)
      );
      break;
    }
    case SPN_EVENT_TARGET_BUILD: {
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

static u32 sp_str_visual_len(sp_str_t str) {
  u32 visual_len = 0;
  bool in_escape = false;

  for (u32 it = 0; it < str.len; it++) {
    if (str.data[it] == '\033') {
      in_escape = true;
    } else if (in_escape && str.data[it] == 'm') {
      in_escape = false;
    } else if (!in_escape) {
      visual_len++;
    }
  }

  return visual_len;
}

static sp_str_t sp_str_visual_pad(sp_str_t str, u32 target_visual_width) {
  u32 current_visual_len = sp_str_visual_len(str);
  s32 delta = (s32)target_visual_width - (s32)current_visual_len;

  if (delta <= 0) {
    return str;
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&builder, str);
  for (s32 it = 0; it < delta; it++) {
    sp_str_builder_append_c8(&builder, ' ');
  }

  return sp_str_builder_to_str(&builder);
}

void sp_tui_begin_table(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_NONE);

  table->cols = SP_NULLPTR;
  table->rows = SP_NULLPTR;
  table->cursor = (sp_tui_cursor_t) { .row = 0, .col = 0 };
  table->state = SP_TUI_TABLE_SETUP;
  table->columns = 0;
  table->indent = 0;
}

void sp_tui_table_setup_column(sp_tui_table_t* table, sp_str_t name) {
  sp_tui_table_setup_column_ex(table, name, 0);
}

void sp_tui_table_setup_column_ex(sp_tui_table_t* table, sp_str_t name, u32 min_width) {
  SP_ASSERT(table->state == SP_TUI_TABLE_SETUP);
  sp_tui_column_t col = { .name = name, .min_width = min_width };
  sp_dyn_array_push(table->cols, col);
  table->columns++;
}

void sp_tui_table_header_row(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_SETUP);
  SP_ASSERT(table->columns > 0);
  table->state = SP_TUI_TABLE_BUILDING;
  table->cursor.row = 0;
  table->cursor.col = 0;
}

void sp_tui_table_next_row(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);

  table->cursor.row = sp_dyn_array_size(table->rows);
  sp_da(sp_str_t) new_row = SP_NULLPTR;
  sp_dyn_array_push(table->rows, new_row);
  table->cursor.col = 0;
}

void sp_tui_table_column(sp_tui_table_t* table, u32 n) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);
  SP_ASSERT(n < table->columns);
  table->cursor.col = n;
}

void sp_tui_table_column_named(sp_tui_table_t* table, sp_str_t name) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);

  sp_dyn_array_for(table->cols, it) {
    if (sp_str_equal(table->cols[it].name, name)) {
      table->cursor.col = it;
      return;
    }
  }

  SP_ASSERT(false && "Column name not found");
}

void sp_tui_table_str(sp_tui_table_t* table, sp_str_t str) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);
  SP_ASSERT(table->cursor.col < table->columns);
  SP_ASSERT(table->cursor.row < sp_dyn_array_size(table->rows));

  sp_da(sp_str_t)* row = &table->rows[table->cursor.row];

  while (sp_dyn_array_size(*row) <= table->cursor.col) {
    sp_dyn_array_push(*row, sp_str_lit(""));
  }

  (*row)[table->cursor.col] = str;
  table->cursor.col++;
}

void sp_tui_table_fmt(sp_tui_table_t* table, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);

  sp_tui_table_str(table, str);
}

void sp_tui_table_set_indent(sp_tui_table_t* table, u32 indent) {
  table->indent = indent;
}

void sp_tui_table_end(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);
  table->state = SP_TUI_TABLE_NONE;
}

static void sp_tui_apply_indent(sp_str_builder_t* builder, u32 indent) {
  for (u32 it = 0; it < indent; it++) {
    sp_str_builder_append_c8(builder, ' ');
    sp_str_builder_append_c8(builder, ' ');
  }
}

sp_str_t sp_tui_table_render(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_NONE);

  if (table->columns == 0) {
    return sp_str_lit("");
  }

  sp_da(u32) widths = SP_NULLPTR;
  for (u32 col = 0; col < table->columns; col++) {
    u32 max_width = table->cols[col].min_width;

    sp_dyn_array_for(table->rows, row_idx) {
      sp_da(sp_str_t)* row = &table->rows[row_idx];
      if (col < sp_dyn_array_size(*row)) {
        max_width = SP_MAX(max_width, sp_str_visual_len((*row)[col]));
      }
    }

    sp_dyn_array_push(widths, max_width);
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  sp_dyn_array_for(table->rows, row_idx) {
    sp_da(sp_str_t)* row = &table->rows[row_idx];
    sp_tui_apply_indent(&builder, table->indent);

    for (u32 col = 0; col < table->columns; col++) {
      sp_str_t cell = (col < sp_dyn_array_size(*row)) ? (*row)[col] : sp_str_lit("");
      sp_str_t padded = sp_str_visual_pad(cell, widths[col]);

      sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(padded));

      if (col < table->columns - 1) {
        sp_str_builder_append_c8(&builder, ' ');
      }
    }
    sp_str_builder_new_line(&builder);
  }

  return sp_str_builder_to_str(&builder);
}
