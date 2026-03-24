#ifndef SPN_TUI_TYPES_H
#define SPN_TUI_TYPES_H

#include "sp.h"

#if defined(SP_POSIX)
  #include <termios.h>
#endif

#define SPN_SPINNER_TRAIL_LEN 6
#define SPN_SPINNER_CYCLE_MS 500.0f
#define SPN_SPINNER_HOLD_START_MS 500.0f
#define SPN_SPINNER_HOLD_END_MS 100.0f

#define SPN_SPINNER_ACTIVE "\u25A0"
#define SPN_SPINNER_INACTIVE "\u2B1D"

typedef struct spn_spinner_t {
  sp_interp_t interp;
  bool forward;
  bool render_forward;
  f32 hold_ms;
  f32 hold_total_ms;
  f32 value;
  u32 width;
  sp_color_t color;
} spn_spinner_t;

#define SPN_OUTPUT_MODE(X) \
  X(SPN_OUTPUT_MODE_INTERACTIVE) \
  X(SPN_OUTPUT_MODE_NONINTERACTIVE) \
  X(SPN_OUTPUT_MODE_QUIET) \
  X(SPN_OUTPUT_MODE_NONE)

typedef enum {
  SPN_OUTPUT_MODE(SP_X_ENUM_DEFINE)
} spn_tui_mode_t;

typedef enum {
  SPN_VERBOSITY_QUIET,
  SPN_VERBOSITY_NORMAL,
  SPN_VERBOSITY_VERBOSE,
  SPN_VERBOSITY_DEBUG,
} spn_verbosity_t;

typedef enum {
  SP_TUI_TABLE_NONE,
  SP_TUI_TABLE_SETUP,
  SP_TUI_TABLE_BUILDING,
} sp_tui_table_state_t;

typedef struct {
  u32 row;
  u32 col;
} sp_tui_cursor_t;

typedef struct {
  sp_str_t name;
  u32 min_width;
} sp_tui_column_t;

typedef struct {
  sp_da(sp_tui_column_t) cols;
  sp_da(sp_da(sp_str_t)) rows;
  sp_tui_cursor_t cursor;
  sp_tui_table_state_t state;
  u32 columns;
  u32 indent;
} sp_tui_table_t;

typedef struct {
  spn_tui_mode_t mode;
  u32 num_deps;
  u32 width;
  sp_ht(sp_str_t, s32) state;
  void* build;
  spn_spinner_t spinner;

  struct {
    u32 max_name;
  } info;

  struct {
    sp_tm_timer_t timer;
    u64 accumulated;
  } frame;

  struct {
#ifdef SP_WIN32
    sp_win32_dword_t original_input_mode;
    sp_win32_dword_t original_output_mode;
    sp_win32_handle_t input_handle;
    sp_win32_handle_t output_handle;
#else
    struct termios ios;
#endif
    bool modified;
  } terminal;

  sp_tui_table_t table;
} spn_tui_t;

#endif
