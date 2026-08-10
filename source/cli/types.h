#ifndef SPN_CLI_TYPES_H
#define SPN_CLI_TYPES_H

#include "sp.h"
#include "sp/sp_cli.h"

#include "spn/host.h"

typedef struct spn_cli spn_cli_t;

typedef struct {
  sp_str_t name;
  sp_str_t toolchain;
  sp_str_t mode;
  sp_str_t opt;
  sp_str_t sanitize;
  sp_str_t target;
  sp_str_t os;
  sp_str_t arch;
  sp_str_t abi;
} spn_cli_profile_t;

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
    bool example;
  } only;
} spn_cli_build_t;

typedef struct {
  sp_str_t entry;
} spn_cli_run_t;

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
  u32 refresh;

  spn_cli_profile_t profile;

  spn_cli_add_t add;
  spn_cli_init_t init;
  spn_cli_build_t build;
  spn_cli_run_t run;
  spn_cli_publish_t publish;
  spn_cli_index_t index;
};

typedef struct {
  spn_ctx_t* ctx;
  spn_session_t* session;
  sp_mem_t mem;
  struct {
    sp_sys_fd_t read;
    sp_sys_fd_t write;
  } doorbell;
  sp_atomic_s32_t interrupted;
} spn_cli_host_t;

extern spn_cli_t args;
extern spn_cli_host_t host;

#endif
