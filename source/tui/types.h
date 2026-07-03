#ifndef SPN_TUI_TYPES_H
#define SPN_TUI_TYPES_H

#include "sp/macro.h"

#include "sp.h"
#include "sp/sp_math.h"
#include "sp/prompt.h"
#include "forward/types.h"

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

typedef struct {
  sp_io_writer_t base;
  sp_prompt_ctx_t* prompt;
  sp_io_writer_t* downstream;
  sp_da(c8) partial;
  u32 deferred_blanks;
} spn_tui_line_writer_t;

typedef struct {
  spn_tui_mode_t mode;
  spn_session_t* session;
  u32 num_deps;
  u32 width;
  sp_ht(sp_str_t, s32) state;
  void* build;
  sp_io_writer_t* out;
  spn_tui_line_writer_t line_writer;

  struct {
    sp_prompt_ctx_t* ctx;
    sp_app_t app;
    sp_prompt_widget_t widget;
    bool started;
    bool on;
  } prompt;

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
} spn_tui_t;

#endif
