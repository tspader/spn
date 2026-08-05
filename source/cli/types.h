#ifndef SPN_CLI_TYPES_H
#define SPN_CLI_TYPES_H

#include "sp.h"
#include "sp/sp_cli.h"

#include "error/types.h"
#include "profile/types.h"

typedef struct spn_cli spn_cli_t;

typedef struct {
  spn_err_union_t result;
  spn_err_union_t (*finish)();
} spn_cli_exec_t;

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

extern spn_cli_t args;

#endif
