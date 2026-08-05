#ifndef SPN_TUI_TYPES_H
#define SPN_TUI_TYPES_H

#include "sp/macro.h"

#include "sp.h"
#include "sp/sp_math.h"
#include "sp/sp_prompt.h"
#include "forward/types.h"
#include "log/types.h"

#define SPN_OUTPUT_MODE(X) \
  X(SPN_OUTPUT_MODE_INTERACTIVE) \
  X(SPN_OUTPUT_MODE_NONINTERACTIVE) \
  X(SPN_OUTPUT_MODE_QUIET) \
  X(SPN_OUTPUT_MODE_NONE) \
  X(SPN_OUTPUT_MODE_JSON)

typedef enum {
  SPN_OUTPUT_MODE(SP_X_ENUM_DEFINE)
} spn_tui_mode_t;

typedef struct {
  sp_io_writer_t base;
  sp_prompt_ctx_t* prompt;
  sp_io_writer_t* downstream;
  sp_da(c8) partial;
  u32 deferred_blanks;
} spn_tui_line_writer_t;

typedef struct {
  spn_tui_mode_t mode;
  sp_mem_t mem;
  spn_logger_t* logger;
  u32 num_deps;
  u32 width;
  sp_ht(sp_str_t, s32) state;
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
} spn_tui_t;

#endif
