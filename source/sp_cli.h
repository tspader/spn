#ifndef SP_CLI_H
#define SP_CLI_H

#include "sp.h"

#define SP_CLI_MAX_ARGS 8
#define SP_CLI_MAX_OPTS 8
#define SP_CLI_MAX_SUBCOMMANDS 16
#define SP_CLI_MAX_SUBCOMMANDS_NESTED 8
#define SP_CLI_NO_PLACEHOLDER SP_NULLPTR
#define SP_CLI_NO_SUMMARY SP_NULLPTR

#define SP_CLI_ARG_KIND(X) \
  X(SP_CLI_ARG_KIND_REQUIRED, "required") \
  X(SP_CLI_ARG_KIND_OPTIONAL, "optional")

typedef enum {
  SP_CLI_ARG_KIND(SP_X_NAMED_ENUM_DEFINE)
} sp_cli_arg_kind_t;

#define SP_CLI_OPT_KIND(X) \
  X(SP_CLI_OPT_KIND_BOOLEAN, "boolean") \
  X(SP_CLI_OPT_KIND_STRING, "string") \
  X(SP_CLI_OPT_KIND_INTEGER, "integer")

typedef enum {
  SP_CLI_OPT_KIND(SP_X_NAMED_ENUM_DEFINE)
} sp_cli_opt_kind_t;

typedef struct {
  const c8* name;
  sp_cli_arg_kind_t kind;
  const c8* summary;
  void* ptr;
} sp_cli_arg_usage_t;

typedef struct {
  const c8* brief;
  const c8* name;
  sp_cli_opt_kind_t kind;
  const c8* summary;
  const c8* placeholder;
  void* ptr;
} sp_cli_opt_usage_t;

typedef struct sp_cli_subcommand_usage_t sp_cli_subcommand_usage_t;

typedef struct {
  const c8* name;
  const c8* usage;
  const c8* summary;
  sp_cli_opt_usage_t opts[SP_CLI_MAX_OPTS];
  sp_cli_arg_usage_t args[SP_CLI_MAX_ARGS];
  sp_cli_subcommand_usage_t* subcommands;
  void* ptr;
  s32 bind_val;
} sp_cli_command_usage_t;

struct sp_cli_subcommand_usage_t {
  const c8* usage;
  const c8* summary;
  sp_cli_command_usage_t commands[SP_CLI_MAX_SUBCOMMANDS_NESTED];
};

typedef struct {
  const c8* usage;
  const c8* summary;
  sp_cli_opt_usage_t opts[SP_CLI_MAX_OPTS];
  sp_cli_command_usage_t commands[SP_CLI_MAX_SUBCOMMANDS];
} sp_cli_usage_t;

typedef struct {
  c8** argv;
  u32 argc;
  sp_cli_command_usage_t cli;
  bool skip_help;
  bool stop_at_non_option;
  u32 it;
  sp_str_t positionals[SP_CLI_MAX_ARGS];
  u32 num_positionals;
  sp_str_t err;
} sp_cli_parser_t;

sp_cli_opt_usage_t* sp_cli_match_opt(sp_cli_command_usage_t* cmd, sp_str_t name);
sp_cli_opt_usage_t* sp_cli_match_brief_opt(sp_cli_command_usage_t* cmd, c8 brief);
sp_str_t sp_cli_parse_opt_value(sp_cli_parser_t* p, sp_str_t rest);
bool sp_cli_parse_long_opt(sp_cli_parser_t* p, sp_str_t arg);
bool sp_cli_parse_short_opts(sp_cli_parser_t* p, sp_str_t arg);
bool sp_cli_bind_positionals(sp_cli_parser_t* p);
bool sp_cli_parse_command(sp_cli_parser_t* p);
bool sp_cli_parse(sp_cli_usage_t* usage, int argc, char** argv, sp_cli_command_usage_t** out_cmd);

static inline bool sp_cli_parser_is_done(sp_cli_parser_t* p) {
  return p->it >= p->argc;
}

static inline sp_str_t sp_cli_parser_peek(sp_cli_parser_t* p) {
  return sp_str_from_cstr(p->argv[p->it]);
}

static inline void sp_cli_parser_eat(sp_cli_parser_t* p) {
  p->it++;
}

static inline bool sp_cli_parser_is_opt(sp_cli_parser_t* p) {
  if (sp_cli_parser_is_done(p)) return false;
  c8* arg = p->argv[p->it];
  return arg[0] == '-';
}

#ifdef SP_CLI_IMPLEMENTATION

sp_cli_opt_usage_t* sp_cli_match_opt(sp_cli_command_usage_t* cmd, sp_str_t name) {
  for (u32 i = 0; i < SP_CLI_MAX_OPTS; i++) {
    sp_cli_opt_usage_t* opt = &cmd->opts[i];
    if (!opt->name && !opt->brief) return NULL;
    if (opt->name && sp_str_equal_cstr(name, opt->name)) {
      return opt;
    }
  }
  return NULL;
}

sp_cli_opt_usage_t* sp_cli_match_brief_opt(sp_cli_command_usage_t* cmd, c8 brief) {
  for (u32 i = 0; i < SP_CLI_MAX_OPTS; i++) {
    sp_cli_opt_usage_t* opt = &cmd->opts[i];
    if (!opt->name && !opt->brief) return NULL;
    if (opt->brief && opt->brief[0] == brief) {
      return opt;
    }
  }
  return NULL;
}

sp_str_t sp_cli_parse_opt_value(sp_cli_parser_t* p, sp_str_t rest) {
  if (rest.len > 0 && rest.data[0] == '=') {
    return sp_str_suffix(rest, rest.len - 1);
  }

  if (sp_cli_parser_is_done(p)) {
    return (sp_str_t){0};
  }

  sp_str_t next = sp_cli_parser_peek(p);
  if (next.data[0] == '-') {
    return (sp_str_t){0};
  }

  sp_cli_parser_eat(p);
  return next;
}

static void sp_cli_assign_bool(void* ptr, bool value) {
  if (ptr) {
    bool* b = (bool*)ptr;
    *b = value;
  }
}

static void sp_cli_assign_str(void* ptr, sp_str_t value) {
  if (ptr) {
    sp_str_t* str = (sp_str_t*)ptr;
    *str = value;
  }
}

static void sp_cli_assign_s64(void* ptr, s64 value) {
  if (ptr) {
    s64* n = (s64*)ptr;
    *n = value;
  }
}

static void sp_cli_assign(sp_cli_opt_usage_t* opt, sp_str_t value) {
  switch (opt->kind) {
    case SP_CLI_OPT_KIND_BOOLEAN:
      sp_cli_assign_bool(opt->ptr, true);
      break;
    case SP_CLI_OPT_KIND_STRING:
      sp_cli_assign_str(opt->ptr, value);
      break;
    case SP_CLI_OPT_KIND_INTEGER:
      sp_cli_assign_s64(opt->ptr, sp_parse_s64(value));
      break;
  }
}

bool sp_cli_parse_long_opt(sp_cli_parser_t* p, sp_str_t arg) {
  sp_cli_command_usage_t* cmd = &p->cli;

  sp_str_t after_dashes = sp_str_suffix(arg, arg.len - 2);

  sp_str_t name = after_dashes;
  sp_str_t rest = (sp_str_t){0};

  sp_str_for(after_dashes, i) {
    if (sp_str_at(after_dashes, i) == '=') {
      name = sp_str_prefix(after_dashes, i);
      rest = sp_str_suffix(after_dashes, after_dashes.len - i);
      break;
    }
  }

  sp_cli_opt_usage_t* opt = sp_cli_match_opt(cmd, name);
  if (!opt) return false;

  sp_cli_parser_eat(p);

  sp_str_t value = (opt->kind == SP_CLI_OPT_KIND_BOOLEAN)
    ? sp_str_lit("true")
    : sp_cli_parse_opt_value(p, rest);

  sp_cli_assign(opt, value);

  return true;
}

bool sp_cli_parse_short_opts(sp_cli_parser_t* p, sp_str_t arg) {
  sp_cli_command_usage_t* cmd = &p->cli;

  sp_str_t chars = sp_str_suffix(arg, arg.len - 1);

  sp_cli_parser_eat(p);

  sp_str_for(chars, i) {
    c8 ch = sp_str_at(chars, i);

    sp_cli_opt_usage_t* opt = sp_cli_match_brief_opt(cmd, ch);
    if (!opt) return false;

    sp_str_t value = (opt->kind == SP_CLI_OPT_KIND_BOOLEAN)
      ? sp_str_lit("true")
      : sp_cli_parse_opt_value(p, (sp_str_t){0});

    sp_cli_assign(opt, value);
  }

  return true;
}

bool sp_cli_bind_positionals(sp_cli_parser_t* p) {
  sp_cli_command_usage_t* cmd = &p->cli;

  for (u32 i = 0; i < p->num_positionals && i < SP_CLI_MAX_ARGS; i++) {
    if (!cmd->args[i].name) break;
    sp_cli_assign_str(cmd->args[i].ptr, p->positionals[i]);
  }

  for (u32 i = 0; i < SP_CLI_MAX_ARGS; i++) {
    sp_cli_arg_usage_t* arg = &cmd->args[i];
    if (!arg->name) break;

    if (arg->kind == SP_CLI_ARG_KIND_REQUIRED) {
      if (p->num_positionals <= i) {
        p->err = sp_format("missing required argument: {}", SP_FMT_CSTR(arg->name));
        return false;
      }
    }
  }

  return true;
}

bool sp_cli_parse_command(sp_cli_parser_t* p) {
  sp_cli_command_usage_t* cmd = &p->cli;

  while (!sp_cli_parser_is_done(p)) {
    sp_str_t arg = sp_cli_parser_peek(p);

    if (sp_cli_parser_is_opt(p)) {
      bool ok;
      if (sp_str_starts_with(arg, sp_str_lit("--"))) {
        ok = sp_cli_parse_long_opt(p, arg);
      } else {
        ok = sp_cli_parse_short_opts(p, arg);
      }

      if (!ok) {
        p->err = sp_format("unknown option: {}", SP_FMT_STR(arg));
        return false;
      }
    }
    else {
      if (p->stop_at_non_option) break;
      p->positionals[p->num_positionals++] = arg;
      sp_cli_parser_eat(p);
    }
  }

  return sp_cli_bind_positionals(p);
}

static void sp_cli_merge_opts(sp_cli_command_usage_t* dest, sp_cli_opt_usage_t* src) {
  int dest_idx = 0;
  while (dest_idx < SP_CLI_MAX_OPTS && dest->opts[dest_idx].name) {
    dest_idx++;
  }

  for (int i = 0; i < SP_CLI_MAX_OPTS; i++) {
    if (!src[i].name) break;
    if (dest_idx >= SP_CLI_MAX_OPTS) break;

    bool collision = false;
    for (int j = 0; j < dest_idx; j++) {
      if (dest->opts[j].name && sp_str_equal_cstr(sp_str_from_cstr(src[i].name), dest->opts[j].name)) {
        collision = true;
        break;
      }
    }

    if (!collision) {
      dest->opts[dest_idx++] = src[i];
    }
  }
}

bool sp_cli_parse(sp_cli_usage_t* usage, int argc, char** argv, sp_cli_command_usage_t** out_cmd) {
  if (out_cmd) *out_cmd = NULL;

  // 1. Parse global options
  sp_cli_parser_t p = {
    .argv = (c8**)argv + 1,
    .argc = argc - 1,
    .stop_at_non_option = true,
    .skip_help = true,
    .cli = {
      .name = "spn",
      .summary = usage->summary,
      .usage = usage->usage,
    }
  };
  for (int i = 0; i < SP_CLI_MAX_OPTS; i++) {
    p.cli.opts[i] = usage->opts[i];
  }

  if (!sp_cli_parse_command(&p)) {
    return false;
  }

  if (sp_cli_parser_is_done(&p)) {
    return true;
  }
  sp_str_t subcmd_name = sp_cli_parser_peek(&p);
  sp_cli_command_usage_t* matched_cmd = NULL;

  for (int i = 0; i < SP_CLI_MAX_SUBCOMMANDS; i++) {
    if (!usage->commands[i].name) break;
    if (sp_str_equal_cstr(subcmd_name, usage->commands[i].name)) {
      matched_cmd = &usage->commands[i];
      break;
    }
  }

  if (!matched_cmd) {
    return false;
  }

  sp_cli_parser_eat(&p);

  if (matched_cmd->ptr) {
    *(s32*)matched_cmd->ptr = matched_cmd->bind_val;
  }

  sp_cli_parser_t sub_p = {
    .argv = p.argv,
    .argc = p.argc,
    .it = p.it,
    .stop_at_non_option = matched_cmd->subcommands != NULL,
    .skip_help = false,
    .cli = *matched_cmd
  };
  sp_cli_merge_opts(&sub_p.cli, usage->opts);

  if (!sp_cli_parse_command(&sub_p)) {
    return false;
  }

  p.it = sub_p.it;
  if (matched_cmd->subcommands) {
    if (!sp_cli_parser_is_done(&p)) {
      sp_str_t nested_name = sp_cli_parser_peek(&p);
      sp_cli_command_usage_t* nested_cmd = NULL;

      for (int i = 0; i < SP_CLI_MAX_SUBCOMMANDS_NESTED; i++) {
        if (!matched_cmd->subcommands->commands[i].name) break;
        if (sp_str_equal_cstr(nested_name, matched_cmd->subcommands->commands[i].name)) {
          nested_cmd = &matched_cmd->subcommands->commands[i];
          break;
        }
      }

      if (nested_cmd) {
        sp_cli_parser_eat(&p);

        if (nested_cmd->ptr) {
          *(s32*)nested_cmd->ptr = nested_cmd->bind_val;
        }

        sp_cli_parser_t nested_p = {
          .argv = p.argv,
          .argc = p.argc,
          .it = p.it,
          .stop_at_non_option = false,
          .skip_help = false,
          .cli = *nested_cmd
        };
        sp_cli_merge_opts(&nested_p.cli, usage->opts);

        if (!sp_cli_parse_command(&nested_p)) {
          if (out_cmd) *out_cmd = nested_cmd;
          return false;
        }

        if (out_cmd) *out_cmd = nested_cmd;
        return true;
      } else {
        if (out_cmd) *out_cmd = matched_cmd;
        return false;
      }
    } else {
      if (out_cmd) *out_cmd = matched_cmd;
      return false;
    }
  }

  if (out_cmd) *out_cmd = matched_cmd;
  return true;
}

#endif

#endif
