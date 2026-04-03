#ifndef SPN_CLI_TYPES_H
#define SPN_CLI_TYPES_H

#include "sp.h"
#include "sp/cli.h"

#define SPN_CLI_COMMAND(X) \
  X(SPN_CLI_INIT, "init") \
  X(SPN_CLI_ADD, "add") \
  X(SPN_CLI_BUILD, "build") \
  X(SPN_CLI_RUN, "run") \
  X(SPN_CLI_TEST, "test") \
  X(SPN_CLI_CLEAN, "clean") \
  X(SPN_CLI_GENERATE, "generate") \
  X(SPN_CLI_COPY, "copy") \
  X(SPN_CLI_UPDATE, "update") \
  X(SPN_CLI_LIST, "list") \
  X(SPN_CLI_WHICH, "which") \
  X(SPN_CLI_LS, "ls") \
  X(SPN_CLI_MANIFEST, "manifest") \
  X(SPN_CLI_TOOL, "tool") \
  X(SPN_CLI_PUBLISH, "publish")

typedef enum {
  SPN_CLI_COMMAND(SP_X_NAMED_ENUM_DEFINE)
} spn_cli_cmd_t;

#define SPN_TOOL_SUBCOMMAND(X) \
  X(SPN_TOOL_INSTALL, "install") \
  X(SPN_TOOL_UNINSTALL, "uninstall") \
  X(SPN_TOOL_RUN, "run") \
  X(SPN_TOOL_LIST, "list") \
  X(SPN_TOOL_UPDATE, "update")

typedef enum {
  SPN_TOOL_SUBCOMMAND(SP_X_NAMED_ENUM_DEFINE)
} spn_tool_cmd_t;

typedef struct {
  sp_str_t package;
  bool test;
  bool build;
} spn_cli_add_t;

typedef struct {
  sp_str_t package;
} spn_cli_update_t;

typedef struct {
  union {
    sp_str_t package;
    sp_str_t dir;
  };
  bool force;
  sp_str_t version;
} spn_cli_tool_install_t;

typedef struct {
  sp_str_t package;
  sp_str_t command;
} spn_cli_tool_run_t;

typedef struct {
  spn_tool_cmd_t subcommand;
  union {
    spn_cli_tool_install_t install;
    spn_cli_tool_run_t run;
  };
} spn_cli_tool_t;

typedef struct {
  bool bare;
} spn_cli_init_t;

typedef struct {
  bool force;
  bool tests;
  sp_str_t target;
  sp_str_t profile;
  sp_str_t toolchain;
  sp_str_t mode;
} spn_cli_build_t;

typedef struct {
  sp_str_t entry;
  sp_str_t profile;
  sp_str_t toolchain;
  sp_str_t mode;
} spn_cli_run_t;

typedef struct {
  sp_str_t target;
  sp_str_t profile;
  sp_str_t toolchain;
  sp_str_t mode;
} spn_cli_test_t;

typedef struct {
  sp_str_t generator;
  sp_str_t compiler;
  sp_str_t path;
} spn_cli_generate_t;

typedef struct {
  sp_str_t dir;
  sp_str_t package;
} spn_cli_which_t;

typedef struct {
  sp_str_t dir;
  sp_str_t package;
} spn_cli_ls_t;

typedef struct {
  sp_str_t package;
} spn_cli_manifest_t;

typedef struct {
  sp_str_t directory;
} spn_cli_copy_t;

typedef struct {
  sp_str_t output;
  bool dirty;
} spn_cli_graph_t;

typedef struct {
  sp_str_t index;
  sp_str_t source_url;
  sp_str_t source_rev;
} spn_cli_publish_t;

typedef struct {
  sp_str_t profile;
} spn_cli_clean_t;

struct spn_cli {
  u32 num_args;
  const c8** args;
  sp_str_t project_dir;
  sp_str_t project_file;
  sp_str_t output;
  bool help;
  bool verbose;
  bool quiet;

  spn_cli_usage_t usage;
  spn_cli_add_t add;
  spn_cli_update_t update;
  spn_cli_tool_t tool;
  spn_cli_init_t init;
  spn_cli_generate_t generate;
  spn_cli_build_t build;
  spn_cli_run_t run;
  spn_cli_test_t test;
  spn_cli_ls_t ls;
  spn_cli_which_t which;
  spn_cli_manifest_t manifest;
  spn_cli_copy_t copy;
  spn_cli_graph_t graph;
  spn_cli_clean_t clean;
  spn_cli_publish_t publish;
};

#endif
