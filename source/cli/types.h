#ifndef SPN_CLI_TYPES_H
#define SPN_CLI_TYPES_H

#include "sp.h"
#include "sp/sp_cli.h"

#include "error/types.h"
#include "forward/types.h"
#include "op/types.h"
#include "profile/types.h"
#include "session/types.h"

typedef struct spn_cli spn_cli_t;

typedef struct {
  spn_op_desc_t op;
  spn_session_config_t config;
  spn_err_union_t (*finish)(spn_ctx_t*);
} spn_command_t;

#define SPN_CLI_COMMAND(X) \
  X(SPN_CLI_INIT, "init") \
  X(SPN_CLI_ADD, "add") \
  X(SPN_CLI_BUILD, "build") \
  X(SPN_CLI_RUN, "run") \
  X(SPN_CLI_TEST, "test") \
  X(SPN_CLI_CLEAN, "clean") \
  X(SPN_CLI_PUBLISH, "publish") \
  X(SPN_CLI_INDEX, "index")

typedef enum {
  SPN_CLI_COMMAND(SP_X_NAMED_ENUM_DEFINE)
} spn_cli_cmd_t;

typedef struct {
  sp_str_t package;
  bool test;
  bool build;
} spn_cli_add_t;

typedef struct {
  bool bare;
  sp_str_t path;
} spn_cli_init_t;

typedef struct {
  bool force;
  struct {
    bool test;
    bool bin;
    bool lib;
    bool script;
  } only;
} spn_cli_build_t;

typedef struct {
  sp_str_t entry;
} spn_cli_run_t;

typedef struct {
  sp_str_t name;
} spn_cli_test_t;

typedef struct {
  sp_str_t index;
  sp_str_t source_url;
  sp_str_t source_rev;
  bool dry;
  bool allow_dirty;
} spn_cli_publish_t;

typedef struct {
  sp_str_t name;
} spn_cli_index_t;

struct spn_cli {
  u32 num_args;
  const c8** args;
  sp_str_t project_dir;
  sp_str_t project_file;
  sp_str_t output;
  bool verbose;
  bool quiet;
  bool ci;
  u32 refresh;

  spn_profile_args_t profile;

  spn_cli_add_t add;
  spn_cli_init_t init;
  spn_cli_build_t build;
  spn_cli_run_t run;
  spn_cli_test_t test;
  spn_cli_publish_t publish;
  spn_cli_index_t index;
};

#endif
