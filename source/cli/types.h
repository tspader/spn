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
  spn_app_config_t config;
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

typedef struct {
  const c8* project_dir;
  const c8* project_file;
  const c8* output;
  struct {
    const c8* name;
    const c8* toolchain;
    const c8* mode;
    const c8* opt;
    const c8* sanitize;
    const c8* target;
    const c8* os;
    const c8* arch;
    const c8* abi;
  } profile;
  struct {
    const c8* path;
  } init;
  struct {
    const c8* package;
  } add;
  struct {
    const c8* entry;
  } run;
  struct {
    const c8* name;
  } test;
  struct {
    const c8* index;
    const c8* source_url;
    const c8* source_rev;
  } publish;
  struct {
    const c8* name;
  } index;
} spn_cli_raw_t;

#endif
