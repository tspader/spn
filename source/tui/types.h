#ifndef SPN_TUI_TYPES_H
#define SPN_TUI_TYPES_H

#include "sp/macro.h"

#include "sp.h"
#include "sp/sp_math.h"
#include "sp/sp_prompt.h"
#include "event/types.h"
#include "forward/types.h"

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
  sp_io_stream_writer_t out;
  sp_io_stream_writer_t err;
  sp_io_file_writer_t jsonl;
  spn_verbosity_t verbosity;
} spn_logger_t;

typedef struct {
  sp_io_writer_t base;
  sp_prompt_ctx_t* prompt;
  sp_io_writer_t* downstream;
  sp_da(c8) partial;
  u32 deferred_blanks;
} spn_tui_line_writer_t;

typedef struct {
  sp_str_t pkg;
  sp_str_t message;
} spn_tui_buffered_log_t;

typedef struct {
  spn_tui_mode_t mode;
  sp_mem_t mem;
  spn_logger_t logger;
  spn_tui_line_writer_t writer;
  sp_str_ht(bool) seen_url;
  sp_da(spn_tui_buffered_log_t) buffered_logs;
  u32 num_downloads;
  sp_ht(u64, u32) thread_ids;

  struct {
    sp_prompt_ctx_t* ctx;
    sp_app_t app;
    sp_prompt_widget_t widget;
    bool started;
    bool on;
  } prompt;

  struct {
    sp_atomic_s32_t requested;
    bool granted;
    sp_semaphore_t ready;
  } handoff;
} spn_tui_t;

extern spn_tui_t tui;

#endif
