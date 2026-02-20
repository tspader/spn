#ifndef SPN_CLI_H
#define SPN_CLI_H

#include "sp.h"

#define SPN_CLI_ARGS_DONE SP_ZERO_STRUCT(spn_cli_command_usage_t)

#define SPN_CLI_MAX_SUBCOMMANDS 16
#define SPN_CLI_MAX_SUBCOMMANDS_NESTED 8
#define SPN_CLI_MAX_ARGS 8
#define SPN_CLI_MAX_OPTS 8
#define SPN_CLI_NO_PLACEHOLDER SP_NULLPTR

#define SPN_CLI_ARG_KIND(X) \
  X(SPN_CLI_ARG_KIND_REQUIRED, "required") \
  X(SPN_CLI_ARG_KIND_OPTIONAL, "opional")

typedef enum {
  SPN_CLI_ARG_KIND(SP_X_NAMED_ENUM_DEFINE)
} spn_cli_arg_kind_t;

#define SPN_CLI_OPT_KIND(X) \
  X(SPN_CLI_OPT_KIND_BOOLEAN, "boolean") \
  X(SPN_CLI_OPT_KIND_STRING, "string") \
  X(SPN_CLI_OPT_KIND_INTEGER, "integer")

typedef enum {
  SPN_CLI_OPT_KIND(SP_X_NAMED_ENUM_DEFINE)
} spn_cli_opt_kind_t;

typedef struct {
  const c8* name;
  spn_cli_arg_kind_t kind;
  const c8* summary;
  void* ptr;
} spn_cli_arg_usage_t;

typedef struct {
  const c8* brief;
  const c8* name;
  spn_cli_opt_kind_t kind;
  const c8* summary;
  const c8* placeholder;
  void* ptr;
} spn_cli_opt_usage_t;

typedef struct spn_cli spn_cli_t;
typedef struct spn_cli_command_usage_t spn_cli_command_usage_t;

struct spn_cli_command_usage_t {
  const c8* name;
  const c8* usage;
  const c8* summary;
  spn_cli_opt_usage_t opts [SPN_CLI_MAX_OPTS];
  spn_cli_arg_usage_t args [SPN_CLI_MAX_ARGS];
  spn_cli_command_usage_t* commands;
  sp_app_result_t (*handler)(spn_cli_t*);
};

// Legacy alias - will be removed in phase 5
typedef struct {
  const c8* usage;
  const c8* summary;
  spn_cli_command_usage_t commands [SPN_CLI_MAX_SUBCOMMANDS];
} spn_cli_usage_t;

// Legacy alias - will be removed in phase 5
typedef struct {
  const c8* usage;
  const c8* summary;
  spn_cli_command_usage_t commands [SPN_CLI_MAX_SUBCOMMANDS_NESTED];
} spn_cli_subcommand_usage_t;

typedef struct {
  sp_str_t name;
  spn_cli_arg_kind_t kind;
  sp_str_t summary;
} spn_cli_arg_info_t;

typedef struct {
  sp_str_t brief;
  sp_str_t name;
  spn_cli_opt_kind_t kind;
  sp_str_t summary;
  sp_str_t placeholder;
} spn_cli_opt_info_t;

typedef struct {
  sp_str_t name;
  sp_str_t usage;
  sp_str_t summary;
  sp_da(spn_cli_opt_info_t) opts;
  sp_da(spn_cli_arg_info_t) args;
  sp_da(sp_str_t) brief;
} spn_cli_command_info_t;

typedef struct {
  struct {
    u32 name;
    u32 opts;
    u32 args;
  } width;
  sp_da(spn_cli_command_info_t) commands;
} spn_cli_usage_info_t;

typedef struct {
  const c8** args;
  u32 num_args;
  spn_cli_command_usage_t* cmd;
  spn_cli_command_usage_t* resolved;
  bool stop_at_non_option;
  u32 it;
  sp_str_t positionals[SPN_CLI_MAX_ARGS];
  u32 num_positionals;
  sp_str_t err;
} spn_cli_parser_t;

typedef struct {
  sp_str_t str;
  u32 it;
  bool found;
} spn_cli_str_parser_t;

typedef struct {
  sp_str_t name;
  sp_str_t value;
  bool has_value;
  bool found;
} spn_cli_named_opt_t;

void      spn_cli_print_help(spn_cli_parser_t* parser);
bool      spn_cli_parser_is_done(spn_cli_parser_t* p);
sp_str_t  spn_cli_parser_peek(spn_cli_parser_t* p);
void      spn_cli_parser_eat(spn_cli_parser_t* p);
bool      spn_cli_parser_is_opt(spn_cli_parser_t* p);
bool      spn_cli_str_parser_is_done(spn_cli_str_parser_t* p);
c8        spn_cli_str_parser_peek(spn_cli_str_parser_t* p);
void      spn_cli_str_parser_eat(spn_cli_str_parser_t* p);
sp_str_t  spn_cli_str_parser_rest(spn_cli_str_parser_t* p);
void      spn_cli_assign_bool(void* ptr, bool value);
void      spn_cli_assign_str(void* ptr, sp_str_t value);
void      spn_cli_assign_s64(void* ptr, s64 value);
void      spn_cli_assign(spn_cli_opt_usage_t opt, sp_str_t value);
sp_app_result_t spn_cli_parse_opts(spn_cli_parser_t* p, spn_cli_command_usage_t* cmd);
sp_app_result_t spn_cli_parse(spn_cli_parser_t* p);
sp_app_result_t spn_cli_dispatch(spn_cli_parser_t* p, spn_cli_t* user_data);
sp_app_result_t spn_cli_run(spn_cli_command_usage_t* cmd, spn_cli_t* user_data, const c8** args, u32 num_args);
sp_str_t  spn_cli_usage(spn_cli_command_usage_t* cmd);
sp_str_t  spn_cli_command_usage(spn_cli_command_usage_t cmd);
sp_str_t  spn_cli_arg_kind_to_str(spn_cli_arg_kind_t kind);
spn_cli_arg_kind_t spn_cli_arg_kind_from_str(sp_str_t str);

#endif
