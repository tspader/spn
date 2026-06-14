#include "sp/cli.h"

sp_app_result_t spn_cli_parser_err(spn_cli_parser_t* parser, sp_str_t err) {
  parser->err = sp_str_copy(spn_allocator, err);
  return SP_APP_ERR;
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

sp_app_result_t spn_cli_parse_opts(spn_cli_parser_t* p, spn_cli_usage_t* cmd) {
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
              sp_str_t value = SP_ZERO_STRUCT(sp_str_t);
              if (opt.kind != SPN_CLI_OPT_KIND_BOOLEAN) {
                if (spn_cli_str_parser_is_done(&ap)) {
                  value = spn_cli_parser_peek(p);
                  spn_cli_parser_eat(p);
                } else {
                  value = spn_cli_str_parser_rest(&ap);
                }
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

  return SP_APP_CONTINUE;
}

sp_app_result_t spn_cli_parse(spn_cli_parser_t* p) {
  spn_cli_usage_t* cmd = p->cmd;

  p->stop_at_non_option = (cmd->commands != SP_NULLPTR);
  s32 err = spn_cli_parse_opts(p, cmd);
  if (err) return err;

  if (cmd->commands && (p->num_args - p->it) >= 1) {
    sp_str_t subcmd_name = sp_str_view(p->args[p->it]);

    for (spn_cli_usage_t* subcmd = cmd->commands; subcmd->name; subcmd++) {
      if (sp_str_equal_cstr(subcmd_name, subcmd->name)) {
        p->it++;
        p->cmd = subcmd;
        return spn_cli_parse(p);
      }
    }

    return SP_APP_ERR;
  }

  p->resolved = cmd;
  return SP_APP_CONTINUE;
}

sp_app_result_t spn_cli_dispatch(spn_cli_parser_t* p, spn_cli_t* user_data) {
  if (p->resolved->handler) {
    return p->resolved->handler(user_data);
  }
  return SP_APP_ERR;
}
