#include "install/plan.h"

#include "sp.h"
#include "os/os.h"
#include "str/str.h"

static bool path_equal(spn_install_os_t os, sp_str_t a, sp_str_t b) {
  switch (os) {
    case SPN_INSTALL_OS_UNIX: return sp_str_equal(a, b);
    case SPN_INSTALL_OS_WINDOWS: return sp_str_iequal(a, b);
  }
  return false;
}

// @spader sp_fs_normalize_path?
static sp_str_t get_home_path(sp_mem_t mem, spn_install_os_t os, sp_env_t* env) {
  switch (os) {
    case SPN_INSTALL_OS_UNIX: {
      sp_str_t home = sp_env_get(env, sp_str_lit("HOME"));
      return sp_str_empty(home) ? home : sp_fs_normalize_path(mem, home);
    }
    case SPN_INSTALL_OS_WINDOWS: {
      // @spader simpler?
      struct { sp_str_t profile; sp_str_t drive; sp_str_t path; } e = {
        sp_env_get_c(env, "USERPROFILE"),
        sp_env_get_c(env, "HOMEDRIVE"),
        sp_env_get_c(env, "HOMEPATH"),
      };
      if (!sp_str_empty(e.profile)) {
        return sp_fs_normalize_path(mem, e.profile);
      }
      if (sp_str_empty(e.drive) || sp_str_empty(e.path)) {
        return sp_zero_s(sp_str_t);
      }
      return sp_str_concat(mem, e.drive, sp_fs_normalize_path(mem, e.path));
    }
  }
  return sp_zero_s(sp_str_t);
}

static sp_str_t to_native(sp_mem_t mem, spn_install_os_t os, sp_str_t path) {
  switch (os) {
    case SPN_INSTALL_OS_UNIX: return path;
    case SPN_INSTALL_OS_WINDOWS: return sp_str_replace_c8(mem, path, '/', '\\');
  }
  return path;
}

static void resolve_rc(sp_mem_t mem, sp_env_t* env, sp_str_t home, spn_install_layout_t* layout) {
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".profile")), .always = true, .shell = SPN_INSTALL_SHELL_BASH };
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".bashrc")), .shell = SPN_INSTALL_SHELL_BASH };
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".bash_profile")), .shell = SPN_INSTALL_SHELL_BASH };
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".bash_login")), .shell = SPN_INSTALL_SHELL_BASH };

  sp_str_t zdotdir = sp_env_get(env, sp_str_lit("ZDOTDIR"));
  zdotdir = sp_str_empty(zdotdir) ? home : sp_fs_normalize_path(mem, zdotdir);
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, zdotdir, sp_str_lit(".zshrc")), .always = true, .shell = SPN_INSTALL_SHELL_ZSH };

  sp_str_t config = sp_env_get(env, sp_str_lit("XDG_CONFIG_HOME"));
  config = sp_str_empty(config) ? sp_fs_join_path(mem, home, sp_str_lit(".config")) : sp_fs_normalize_path(mem, config);
  layout->fish_conf = sp_fs_join_path(mem, config, sp_str_lit("fish/conf.d/spn.fish"));
}

static void resolve_path(sp_mem_t mem, spn_install_os_t os, sp_env_t* env, spn_install_layout_t* layout) {
  c8 sep = os == SPN_INSTALL_OS_WINDOWS ? ';' : ':';
  sp_str_t exe_name = os == SPN_INSTALL_OS_WINDOWS ? sp_str_lit("spn.exe") : sp_str_lit("spn");

  layout->shadows = sp_da_new(mem, sp_str_t);
  sp_da(sp_str_t) entries = sp_str_split_c8(mem, sp_env_get_path(env), sep);
  sp_da_for(entries, it) {
    if (sp_str_empty(entries[it])) {
      continue;
    }
    sp_str_t entry = sp_fs_normalize_path(mem, entries[it]);
    if (path_equal(os, entry, layout->bin)) {
      layout->on_path = true;
    }
    else if (!layout->on_path) {
      sp_da_push(layout->shadows, sp_fs_join_path(mem, entry, exe_name));
    }
  }
}

spn_install_layout_t spn_install_resolve(sp_mem_t mem, spn_install_os_t os, sp_env_t* env) {
  spn_install_layout_t layout = { .os = os };

  layout.home = get_home_path(mem, os, env);
  if (sp_str_empty(layout.home)) {
    layout.err = SPN_INSTALL_ERR_NO_HOME;
    return layout;
  }

  sp_str_t root = sp_fs_join_path(mem, layout.home, sp_str_lit(".spn"));
  layout.bin = sp_fs_join_path(mem, root, sp_str_lit("bin"));
  layout.bin_native = to_native(mem, os, layout.bin);
  layout.exe = sp_fs_join_path(mem, layout.bin, os == SPN_INSTALL_OS_WINDOWS ? sp_str_lit("spn.exe") : sp_str_lit("spn"));

  if (os == SPN_INSTALL_OS_UNIX) {
    layout.env_file = sp_fs_join_path(mem, root, sp_str_lit("env"));
    resolve_rc(mem, env, layout.home, &layout);
  }

  resolve_path(mem, os, env, &layout);

  sp_str_t github = sp_env_get(env, sp_str_lit("GITHUB_PATH"));
  layout.github_path = sp_str_empty(github) ? github : sp_fs_normalize_path(mem, github);
  layout.no_modify_path = !sp_str_empty(sp_env_get(env, sp_str_lit("SPN_INSTALL_NO_MODIFY_PATH")));

  return layout;
}

#define ENV_SH \
  "case \":${PATH}:\" in\n" \
  "  *:\"" SPN_INSTALL_ROOT_EXPR "/bin\":*) ;;\n" \
  "  *) export PATH=\"" SPN_INSTALL_ROOT_EXPR "/bin:${PATH}\" ;;\n" \
  "esac\n"

#define FISH_SH \
  "if not contains \"" SPN_INSTALL_ROOT_EXPR "/bin\" $PATH\n" \
  "  set --export PATH \"" SPN_INSTALL_ROOT_EXPR "/bin\" $PATH\n" \
  "end\n"

#define RC_APPEND "\n" SPN_INSTALL_RC_LINE "\n"

static bool registry_contains(sp_str_t value, sp_str_t bin_native) {
  sp_str_t remaining = value;
  while (remaining.len) {
    s32 sep = sp_str_find_c8(remaining, ';');
    sp_str_t entry = sep < 0 ? remaining : sp_str_prefix(remaining, sep);
    if (sp_str_iequal(entry, bin_native)) {
      return true;
    }
    if (sep < 0) {
      break;
    }
    remaining = sp_str_sub(remaining, sep + 1, (s32)remaining.len - sep - 1);
  }
  return false;
}

spn_install_choices_t spn_install_choices(spn_install_layout_t* layout) {
  spn_install_choices_t choices = sp_zero;
  switch (layout->os) {
    case SPN_INSTALL_OS_UNIX: {
      choices.path[choices.num_path++] = (spn_install_path_choice_t) { .kind = SPN_INSTALL_SHELL_BASH };
      choices.path[choices.num_path++] = (spn_install_path_choice_t) { .kind = SPN_INSTALL_SHELL_ZSH };
      choices.path[choices.num_path++] = (spn_install_path_choice_t) { .kind = SPN_INSTALL_SHELL_FISH };
      break;
    }
    case SPN_INSTALL_OS_WINDOWS: {
      choices.registry = true;
      break;
    }
  }
  return choices;
}

static bool has_shell(spn_install_choices_t* choices, spn_install_shell_t kind) {
  sp_for(it, choices->num_path) {
    if (choices->path[it].kind == kind) {
      return true;
    }
  }
  return false;
}

static spn_install_path_state_t path_state(spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_choices_t* choices) {
  if (layout->on_path) {
    return SPN_INSTALL_PATH_OK;
  }
  if (layout->no_modify_path) {
    return SPN_INSTALL_PATH_MANUAL;
  }
  if (!sp_str_empty(layout->github_path)) {
    return SPN_INSTALL_PATH_CI;
  }
  switch (layout->os) {
    case SPN_INSTALL_OS_UNIX: {
      return choices->num_path ? SPN_INSTALL_PATH_UPDATED : SPN_INSTALL_PATH_MANUAL;
    }
    case SPN_INSTALL_OS_WINDOWS: {
      if (!choices->registry) {
        return SPN_INSTALL_PATH_MANUAL;
      }
      return facts->registry.kind == SPN_INSTALL_REG_OTHER ? SPN_INSTALL_PATH_MANUAL : SPN_INSTALL_PATH_UPDATED;
    }
  }
  return SPN_INSTALL_PATH_MANUAL;
}

static void push_action(spn_install_plan_t* plan, spn_install_action_t action) {
  SP_ASSERT(plan->count < SPN_INSTALL_MAX_ACTIONS);
  SP_ASSERT(action.role != SPN_INSTALL_ROLE_NONE);
  plan->actions[plan->count++] = action;
}

static spn_install_action_t exe_action(spn_install_layout_t* layout, spn_install_facts_t* facts) {
  return (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_INSTALL_EXE,
    .role = SPN_INSTALL_ROLE_EXE,
    .path = layout->exe,
    .src = facts->exe,
  };
}

static spn_install_action_t env_action(spn_install_layout_t* layout) {
  return (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_WRITE_FILE,
    .role = SPN_INSTALL_ROLE_ENV,
    .path = layout->env_file,
    .text = sp_str_lit(ENV_SH),
  };
}

static spn_install_action_t fish_action(spn_install_layout_t* layout) {
  return (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_WRITE_FILE,
    .role = SPN_INSTALL_ROLE_HOOK,
    .path = layout->fish_conf,
    .text = sp_str_lit(FISH_SH),
  };
}

static spn_install_action_t rc_action(sp_str_t path) {
  return (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_APPEND_LINE,
    .role = SPN_INSTALL_ROLE_HOOK,
    .path = path,
    .text = sp_str_lit(RC_APPEND),
    .line = sp_str_lit(SPN_INSTALL_RC_LINE),
  };
}

static spn_install_action_t github_action(sp_mem_t mem, spn_install_layout_t* layout) {
  return (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_APPEND_LINE,
    .role = SPN_INSTALL_ROLE_HOOK,
    .path = layout->github_path,
    .text = sp_fmt(mem, "{}\n", sp_fmt_str(layout->bin_native)).value,
    .line = layout->bin_native,
  };
}

static spn_install_action_t registry_action(sp_mem_t mem, spn_install_layout_t* layout, spn_install_facts_t* facts) {
  sp_str_t value = layout->bin_native;
  if (!sp_str_empty(facts->registry.path)) {
    value = sp_fmt(mem, "{};{}", sp_fmt_str(layout->bin_native), sp_fmt_str(facts->registry.path)).value;
  }
  return (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_SET_USER_PATH,
    .role = SPN_INSTALL_ROLE_HOOK,
    .text = value,
    .reg = facts->registry.kind == SPN_INSTALL_REG_EXPAND ? SPN_INSTALL_REG_EXPAND : SPN_INSTALL_REG_SZ,
  };
}

static bool planned(spn_install_plan_t* plan, sp_str_t path) {
  sp_for(it, plan->count) {
    if (sp_str_equal(plan->actions[it].path, path)) {
      return true;
    }
  }
  return false;
}

static void plan_unix_hooks(spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_choices_t* choices, spn_install_plan_t* plan) {
  if (has_shell(choices, SPN_INSTALL_SHELL_FISH) && !sp_str_empty(layout->fish_conf)) {
    push_action(plan, fish_action(layout));
  }

  sp_for(it, layout->num_rc) {
    if (!has_shell(choices, layout->rc[it].shell)) {
      continue;
    }
    bool wanted = layout->rc[it].always || facts->rc[it].exists;
    if (wanted && !facts->rc[it].has_line) {
      push_action(plan, rc_action(layout->rc[it].path));
    }
  }
  sp_for(it, choices->num_path) {
    spn_install_path_choice_t* choice = &choices->path[it];
    if (choice->kind != SPN_INSTALL_SHELL_CUSTOM || sp_str_empty(choice->custom)) {
      continue;
    }
    if (!choice->has_line && !planned(plan, choice->custom)) {
      push_action(plan, rc_action(choice->custom));
    }
  }
}

spn_install_plan_t spn_install_plan(sp_mem_t mem, spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_choices_t* choices) {
  spn_install_plan_t plan = sp_zero;

  if (!path_equal(layout->os, facts->exe, layout->exe)) {
    push_action(&plan, exe_action(layout, facts));
  }

  plan.state = path_state(layout, facts, choices);
  switch (plan.state) {
    case SPN_INSTALL_PATH_OK:
    case SPN_INSTALL_PATH_MANUAL: {
      break;
    }
    case SPN_INSTALL_PATH_CI: {
      switch (layout->os) {
        case SPN_INSTALL_OS_UNIX: {
          push_action(&plan, env_action(layout));
          push_action(&plan, github_action(mem, layout));
          break;
        }
        case SPN_INSTALL_OS_WINDOWS: {
          push_action(&plan, github_action(mem, layout));
          break;
        }
      }
      break;
    }
    case SPN_INSTALL_PATH_UPDATED: {
      switch (layout->os) {
        case SPN_INSTALL_OS_UNIX: {
          push_action(&plan, env_action(layout));
          plan_unix_hooks(layout, facts, choices, &plan);
          break;
        }
        case SPN_INSTALL_OS_WINDOWS: {
          if (!registry_contains(facts->registry.path, layout->bin_native)) {
            push_action(&plan, registry_action(mem, layout, facts));
          }
          break;
        }
      }
      break;
    }
  }
  return plan;
}

static bool path_broken(spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_plan_t* plan, spn_install_result_t* result) {
  u32 hooks = 0;
  sp_for(it, plan->count) {
    if (plan->actions[it].role == SPN_INSTALL_ROLE_HOOK) {
      hooks++;
    }
  }

  if (plan->state == SPN_INSTALL_PATH_UPDATED) {
    switch (layout->os) {
      case SPN_INSTALL_OS_UNIX: {
        sp_for(it, layout->num_rc) {
          if (facts->rc[it].has_line) {
            hooks++;
          }
        }
        break;
      }
      case SPN_INSTALL_OS_WINDOWS: {
        if (registry_contains(facts->registry.path, layout->bin_native)) {
          hooks++;
        }
        break;
      }
    }
  }

  sp_for(it, result->num_stuck) {
    switch (plan->actions[result->stuck[it]].role) {
      case SPN_INSTALL_ROLE_NONE:
      case SPN_INSTALL_ROLE_EXE: break;
      case SPN_INSTALL_ROLE_ENV: return true;
      case SPN_INSTALL_ROLE_HOOK: hooks--; break;
    }
  }
  return !hooks;
}

bool spn_install_shadowed(spn_install_layout_t* layout, spn_install_facts_t* facts) {
  if (sp_str_empty(facts->shadow)) {
    return false;
  }
  return !path_equal(layout->os, facts->shadow, layout->exe);
}

static void push_msg(spn_install_msgs_t* msgs, spn_install_msg_t msg) {
  SP_ASSERT(msgs->count < SPN_INSTALL_MAX_MSGS);
  msgs->items[msgs->count++] = msg;
}

static void push_stuck(spn_install_msgs_t* msgs, spn_install_action_t* action) {
  switch (action->kind) {
    case SPN_INSTALL_ACTION_NONE:
    case SPN_INSTALL_ACTION_INSTALL_EXE: {
      break;
    }
    case SPN_INSTALL_ACTION_WRITE_FILE: {
      push_msg(msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_STUCK_WRITE, .subject = action->path });
      break;
    }
    case SPN_INSTALL_ACTION_APPEND_LINE: {
      push_msg(msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_STUCK_APPEND, .subject = action->path, .detail = action->line });
      break;
    }
    case SPN_INSTALL_ACTION_SET_USER_PATH: {
      push_msg(msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_STUCK_REGISTRY });
      break;
    }
  }
}

spn_install_msgs_t spn_install_report(spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_plan_t* plan, spn_install_result_t* result) {
  spn_install_msgs_t msgs = sp_zero;

  sp_for(it, result->num_stuck) {
    push_stuck(&msgs, &plan->actions[result->stuck[it]]);
  }

  bool broken = path_broken(layout, facts, plan, result);
  spn_install_msg_t manual = { .kind = SPN_INSTALL_MSG_MANUAL, .subject = layout->bin_native };

  switch (plan->state) {
    case SPN_INSTALL_PATH_OK: {
      break;
    }
    case SPN_INSTALL_PATH_MANUAL: {
      push_msg(&msgs, manual);
      break;
    }
    case SPN_INSTALL_PATH_CI: {
      if (broken) {
        push_msg(&msgs, manual);
      }
      break;
    }
    case SPN_INSTALL_PATH_UPDATED: {
      if (broken) {
        push_msg(&msgs, manual);
        break;
      }
      switch (layout->os) {
        case SPN_INSTALL_OS_UNIX: {
          push_msg(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_RESTART_SHELL, .subject = sp_str_lit(SPN_INSTALL_RC_LINE) });
          break;
        }
        case SPN_INSTALL_OS_WINDOWS: {
          push_msg(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_RESTART_TERMINAL });
          break;
        }
      }
      break;
    }
  }

  if (spn_install_shadowed(layout, facts)) {
    push_msg(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_SHADOW, .subject = facts->shadow, .detail = layout->exe });
  }
  return msgs;
}
