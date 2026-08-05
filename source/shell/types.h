#ifndef SPN_SHELL_TYPES_H
#define SPN_SHELL_TYPES_H

#include "sp.h"

#include "cli/types.h"
#include "codegen/types.h"
#include "codegen/gen/config.gen.h"
#include "error/types.h"
#include "forward/types.h"
#include "log/types.h"
#include "tui/types.h"

struct spn_shell_t {
  spn_cli_t cli;
  spn_tui_t tui;
  sp_app_t* sp;
  s32 num_args;
  const c8** args;
  spn_cg_config_t config_file;
  spn_err_union_t result;

  struct {
    spn_err_union_t (*finish)(spn_shell_t*);
    spn_op_t* op;
  } exec;

  struct {
    sp_io_stream_writer_t out;
    sp_io_stream_writer_t err;
    sp_io_file_writer_t jsonl;
    spn_log_level_t level;
    spn_verbosity_t verbosity;
  } logger;
};

extern spn_shell_t shell;

#endif
