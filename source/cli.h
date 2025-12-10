#ifndef SPN_CLI_H
#define SPN_CLI_H

typedef enum {
  SPN_CLI_DONE,
  SPN_CLI_CONTINUE,
  SPN_CLI_ERR,
} spn_cli_result_t;

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

typedef struct spn_cli_subcommand_usage_t spn_cli_subcommand_usage_t;
typedef struct spn_cli spn_cli_t;

typedef struct {
  const c8* name;
  spn_cli_opt_usage_t opts [SPN_CLI_MAX_OPTS];
  spn_cli_arg_usage_t args [SPN_CLI_MAX_ARGS];
  const c8* usage;
  const c8* summary;
  spn_cli_subcommand_usage_t* subcommands;
  spn_cli_result_t (*handler)(spn_cli_t*);
} spn_cli_command_usage_t;

typedef struct {
  const c8* usage;
  const c8* summary;
  spn_cli_command_usage_t commands [SPN_CLI_MAX_SUBCOMMANDS];
} spn_cli_usage_t;

struct spn_cli_subcommand_usage_t {
  const c8* usage;
  const c8* summary;
  spn_cli_command_usage_t commands [SPN_CLI_MAX_SUBCOMMANDS_NESTED];
};

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
  spn_cli_command_usage_t cli;
  bool skip_help;
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
s32       spn_cli_parse_command(spn_cli_parser_t* p);
sp_str_t  spn_cli_usage(spn_cli_usage_t* cli);
sp_str_t  spn_cli_subcommand_usage(spn_cli_subcommand_usage_t* subcmds, const c8* parent_cmd_name);
sp_str_t  spn_cli_command_usage(spn_cli_command_usage_t cmd);
sp_str_t  spn_cli_arg_kind_to_str(spn_cli_arg_kind_t kind);
spn_cli_arg_kind_t spn_cli_arg_kind_from_str(sp_str_t str);
spn_cli_result_t spn_cli_dispatch(spn_cli_usage_t* cli, spn_cli_t* user_data, const c8** args, u32 num_args);

#ifdef SPN_CLI_IMPLEMENTATION

#ifndef SPN_OK
#define SPN_OK 0
#endif
#ifndef SPN_ERROR
#define SPN_ERROR 1
#endif

s32 spn_cli_parser_err(spn_cli_parser_t* parser, sp_str_t err) {
  parser->err = sp_str_copy(err);
  return SPN_ERROR;
}

bool spn_cli_parser_is_done(spn_cli_parser_t* p) {
  return p->it >= p->num_args;
}

sp_str_t spn_cli_parser_peek(spn_cli_parser_t* p) {
  if (spn_cli_parser_is_done(p)) {
    return SP_ZERO_STRUCT(sp_str_t);
  }
  return sp_str_view(p->args[p->it]);
}

void spn_cli_parser_eat(spn_cli_parser_t* p) {
  p->it++;
}

bool spn_cli_parser_is_opt(spn_cli_parser_t* p) {
  if (spn_cli_parser_is_done(p)) return false;
  const c8* arg = p->args[p->it];
  return arg[0] == '-';
}

bool spn_cli_str_parser_is_done(spn_cli_str_parser_t* p) {
  return p->it >= p->str.len;
}

c8 spn_cli_str_parser_peek(spn_cli_str_parser_t* p) {
  return sp_str_at(p->str, p->it);
}

void spn_cli_str_parser_eat(spn_cli_str_parser_t* p) {
  p->it++;
}

sp_str_t spn_cli_str_parser_rest(spn_cli_str_parser_t* p) {
  return sp_str_sub(p->str, p->it, p->str.len - p->it);
}

void spn_cli_assign_bool(void* ptr, bool value) {
  if (ptr) {
    bool* b = (bool*)ptr;
    *b = value;
  }
}

void spn_cli_assign_str(void* ptr, sp_str_t value) {
  if (ptr) {
    sp_str_t* str = (sp_str_t*)ptr;
    *str = value;
  }
}

void spn_cli_assign_s64(void* ptr, s64 value) {
  if (ptr) {
    s64* n = (s64*)ptr;
    *n = value;
  }
}

void spn_cli_assign(spn_cli_opt_usage_t opt, sp_str_t value) {
  switch (opt.kind) {
    case SPN_CLI_OPT_KIND_BOOLEAN: { spn_cli_assign_bool(opt.ptr, true); break; }
    case SPN_CLI_OPT_KIND_STRING: { spn_cli_assign_str(opt.ptr, value); break; }
    case SPN_CLI_OPT_KIND_INTEGER: { spn_cli_assign_s64(opt.ptr, sp_parse_s64(value)); break; }
  }
}

s32 spn_cli_parse_command(spn_cli_parser_t* p) {
  spn_cli_command_usage_t* cmd = &p->cli;
  while (true) {
    if (spn_cli_parser_is_done(p)) {
      break;
    }

    sp_str_t arg = spn_cli_parser_peek(p);

    if (spn_cli_parser_is_opt(p)) {
      if (sp_str_starts_with(arg, sp_str_lit("--"))) {
        sp_str_t opt_part = sp_str_sub(arg, 2, arg.len - 2);

        spn_cli_named_opt_t option = {
          .name = sp_str_strip_left(arg, sp_str_lit("--"))
        };
        sp_str_for(opt_part, it) {
          if (sp_str_at(opt_part, it) == '=') {
            option.name = sp_str_prefix(opt_part, it);
            option.value = sp_str_suffix(opt_part, opt_part.len - (it + 1));
            option.has_value = true;
            break;
          }
        }

        sp_carr_for(cmd->opts, it) {
          spn_cli_opt_usage_t usage = cmd->opts[it];
          if (!usage.name) break;

          if (sp_str_equal_cstr(option.name, usage.name)) {
            option.found = true;
            spn_cli_parser_eat(p);

            if (!option.has_value) {
              switch (usage.kind) {
                case SPN_CLI_OPT_KIND_BOOLEAN: {
                  option.has_value = true;
                  break;
                }
                case SPN_CLI_OPT_KIND_STRING:
                case SPN_CLI_OPT_KIND_INTEGER: {
                  sp_str_t maybe_value = spn_cli_parser_peek(p);
                  if (!sp_str_starts_with(maybe_value, sp_str_lit("--"))) {
                    option.value = maybe_value;
                    option.has_value = true;
                    spn_cli_parser_eat(p);
                  }
                  break;
                }
              }
            }

            if (option.has_value) {
              spn_cli_assign(usage, option.value);
            }

            break;
          }
        }

        if (!option.found) {
          return spn_cli_parser_err(p, sp_format("Error: unknown option: --{}\n", SP_FMT_STR(option.name)));
        }
      }
      else if (sp_str_starts_with(arg, sp_str_lit("-"))) {
        spn_cli_parser_eat(p);

        spn_cli_str_parser_t ap = {
          .str = sp_str_strip_left(arg, sp_str_lit("-")),
        };

        while (true) {
          if (spn_cli_str_parser_is_done(&ap)) {
            break;
          }

          c8 brief = spn_cli_str_parser_peek(&ap);
          spn_cli_str_parser_eat(&ap);

          sp_carr_for(cmd->opts, i) {
            spn_cli_opt_usage_t opt = cmd->opts[i];
            if (!opt.brief) break;

            if (opt.brief[0] == brief) {
              sp_str_t value;
              if (spn_cli_str_parser_is_done(&ap)) {
                value = spn_cli_parser_peek(p);
                spn_cli_parser_eat(p);
              } else {
                value = spn_cli_str_parser_rest(&ap);
              }
              spn_cli_assign(opt, value);

              ap.found = true;
              break;
            }
          }

          if (!ap.found) {
            return spn_cli_parser_err(p, sp_format("Invalid brief option: {}", SP_FMT_STR(ap.str)));
          }
        }
      }
      else {
        return spn_cli_parser_err(p, sp_format("Invalid option: {}", SP_FMT_STR(arg)));
      }
    }
    else {
      if (p->stop_at_non_option) {
        break;
      }
      p->positionals[p->num_positionals++] = arg;
      spn_cli_parser_eat(p);
    }
  }

  for (u32 i = 0; i < p->num_positionals && i < SPN_CLI_MAX_ARGS; i++) {
    if (!cmd->args[i].name) break;
    spn_cli_assign_str(cmd->args[i].ptr, p->positionals[i]);
  }

  sp_carr_for(cmd->args, it) {
    spn_cli_arg_usage_t arg = cmd->args[it];
    if (!arg.name) break;

    switch (arg.kind) {
      case SPN_CLI_ARG_KIND_REQUIRED: {
        if (p->num_positionals <= it) {
          return spn_cli_parser_err(p, sp_format("Error: missing required argument: {}\n", SP_FMT_CSTR(arg.name)));
        }
        break;
      }
      case SPN_CLI_ARG_KIND_OPTIONAL: {
        break;
      }
    }
  }

  return SPN_OK;
}

spn_cli_result_t spn_cli_dispatch_cmd(spn_cli_command_usage_t* cmd, spn_cli_t* user_data, const c8** args, u32 num_args);

spn_cli_result_t spn_cli_dispatch(spn_cli_usage_t* cli, spn_cli_t* user_data, const c8** args, u32 num_args) {
  if (num_args == 0) {
    return SPN_CLI_ERR;
  }

  sp_str_t cmd_name = sp_str_view(args[0]);

  sp_carr_for(cli->commands, i) {
    spn_cli_command_usage_t* cmd = &cli->commands[i];
    if (!cmd->name) break;

    if (sp_str_equal_cstr(cmd_name, cmd->name)) {
      return spn_cli_dispatch_cmd(cmd, user_data, args + 1, num_args - 1);
    }
  }

  return SPN_CLI_ERR;
}

spn_cli_result_t spn_cli_dispatch_cmd(spn_cli_command_usage_t* cmd, spn_cli_t* user_data, const c8** args, u32 num_args) {
  if (cmd->subcommands) {
    if (num_args < 1) {
      return SPN_CLI_ERR;
    }

    sp_str_t subcmd_name = sp_str_view(args[0]);

    sp_carr_for(cmd->subcommands->commands, j) {
      spn_cli_command_usage_t* subcmd = &cmd->subcommands->commands[j];
      if (!subcmd->name) break;

      if (sp_str_equal_cstr(subcmd_name, subcmd->name)) {
        return spn_cli_dispatch_cmd(subcmd, user_data, args + 1, num_args - 1);
      }
    }

    return SPN_CLI_ERR;
  }

  spn_cli_parser_t parser = {
    .args = args,
    .num_args = num_args,
    .cli = *cmd
  };
  s32 err = spn_cli_parse_command(&parser);
  if (err) return SPN_CLI_ERR;

  if (cmd->handler) {
    return cmd->handler(user_data);
  }
  return SPN_CLI_DONE;
}

#endif
#endif
