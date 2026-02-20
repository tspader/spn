#ifndef SPN_TUI_H
#define SPN_TUI_H

#include "sp.h"
#include "spinner.h"

typedef struct spn_user_node_t spn_user_node_t;
typedef struct spn_build_event_t spn_build_event_t;

#if defined(SP_POSIX)
  #include <termios.h>
#endif

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

void            sp_tui_print(sp_str_t str);
void            sp_tui_up(u32 n);
void            sp_tui_down(u32 n);
void            sp_tui_clear_line(void);
void            sp_tui_show_cursor(void);
void            sp_tui_hide_cursor(void);
void            sp_tui_home(void);
void            sp_tui_flush(void);
void            sp_tui_checkpoint(spn_tui_t* tui);
void            sp_tui_restore(spn_tui_t* tui);
void            sp_tui_setup_raw_mode(spn_tui_t* tui);
void            sp_tui_begin_table(sp_tui_table_t* table);
void            sp_tui_table_setup_column(sp_tui_table_t* table, sp_str_t name);
void            sp_tui_table_setup_column_ex(sp_tui_table_t* table, sp_str_t name, u32 min_width);
void            sp_tui_table_header_row(sp_tui_table_t* table);
void            sp_tui_table_next_row(sp_tui_table_t* table);
void            sp_tui_table_column(sp_tui_table_t* table, u32 n);
void            sp_tui_table_column_named(sp_tui_table_t* table, sp_str_t name);
void            sp_tui_table_fmt(sp_tui_table_t* table, const c8* fmt, ...);
void            sp_tui_table_str(sp_tui_table_t* table, sp_str_t str);
void            sp_tui_table_set_indent(sp_tui_table_t* table, u32 indent);
void            sp_tui_table_end(sp_tui_table_t* table);
sp_str_t        sp_tui_table_render(sp_tui_table_t* table);
void            spn_tui_init(spn_tui_t* tui, spn_tui_mode_t mode);
sp_str_t        spn_tui_render_event(spn_build_event_t* event, u32 max_name);
spn_tui_mode_t  spn_output_mode_from_str(sp_str_t str);
sp_str_t        spn_output_mode_to_str(spn_tui_mode_t mode);

#endif
