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

static bool root_chars_valid(sp_str_t root) {
  sp_str_for(root, it) {
    c8 c = root.data[it];
    if (c == '"' || c == '$' || c == '`' || c == '\\') {
      return false;
    }
  }
  return true;
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
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".profile")), .always = true };
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".bashrc")) };
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".bash_profile")) };
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".bash_login")) };

  sp_str_t zdotdir = sp_env_get(env, sp_str_lit("ZDOTDIR"));
  zdotdir = sp_str_empty(zdotdir) ? home : sp_fs_normalize_path(mem, zdotdir);
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, zdotdir, sp_str_lit(".zshrc")), .always = true };

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

  sp_str_t install = sp_env_get(env, sp_str_lit("SPN_INSTALL"));
  sp_str_t home = get_home_path(mem, os, env);

  if (!sp_str_empty(install)) {
    if (os == SPN_INSTALL_OS_UNIX && !root_chars_valid(install)) {
      layout.err = SPN_INSTALL_ERR_ROOT_CHARS;
      return layout;
    }
    layout.root = sp_fs_normalize_path(mem, install);
    layout.root_expr = layout.root;
  }
  else {
    if (sp_str_empty(home)) {
      layout.err = SPN_INSTALL_ERR_NO_HOME;
      return layout;
    }
    layout.root = sp_fs_join_path(mem, home, sp_str_lit(".spn"));
    layout.root_expr = sp_str_lit("$HOME/.spn");
  }

  layout.bin = sp_fs_join_path(mem, layout.root, sp_str_lit("bin"));
  layout.bin_native = to_native(mem, os, layout.bin);
  layout.exe = sp_fs_join_path(mem, layout.bin, os == SPN_INSTALL_OS_WINDOWS ? sp_str_lit("spn.exe") : sp_str_lit("spn"));

  if (os == SPN_INSTALL_OS_UNIX) {
    layout.env_file = sp_fs_join_path(mem, layout.root, sp_str_lit("env"));
    layout.rc_line = sp_fmt(mem, ". \"{}/env\"", sp_fmt_str(layout.root_expr)).value;
    if (!sp_str_empty(home)) {
      resolve_rc(mem, env, home, &layout);
    }
  }
  else {
    layout.root_expr = sp_zero_s(sp_str_t);
  }

  resolve_path(mem, os, env, &layout);

  sp_str_t github = sp_env_get(env, sp_str_lit("GITHUB_PATH"));
  layout.github_path = sp_str_empty(github) ? github : sp_fs_normalize_path(mem, github);
  layout.no_modify_path = !sp_str_empty(sp_env_get(env, sp_str_lit("SPN_INSTALL_NO_MODIFY_PATH")));

  return layout;
}

static void put(sp_io_writer_t* io, sp_str_t text) {
  sp_io_write_all(io, text.data, text.len, SP_NULLPTR);
}

static sp_str_t env_sh(sp_mem_t mem, sp_str_t root_expr) {
  sp_io_dyn_mem_writer_t body = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &body);
  put(&body.base, sp_str_lit("case \":${PATH}:\" in\n  *:\""));
  put(&body.base, root_expr);
  put(&body.base, sp_str_lit("/bin\":*) ;;\n  *) export PATH=\""));
  put(&body.base, root_expr);
  put(&body.base, sp_str_lit("/bin:${PATH}\" ;;\nesac\n"));
  return sp_io_dyn_mem_writer_as_str(&body);
}

static sp_str_t fish_sh(sp_mem_t mem, sp_str_t root_expr) {
  sp_io_dyn_mem_writer_t body = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &body);
  put(&body.base, sp_str_lit("if not contains \""));
  put(&body.base, root_expr);
  put(&body.base, sp_str_lit("/bin\" $PATH\n  set --export PATH \""));
  put(&body.base, root_expr);
  put(&body.base, sp_str_lit("/bin\" $PATH\nend\n"));
  return sp_io_dyn_mem_writer_as_str(&body);
}

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

static spn_install_path_state_t path_state(spn_install_layout_t* layout, spn_install_facts_t* facts) {
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
      return layout->num_rc ? SPN_INSTALL_PATH_UPDATED : SPN_INSTALL_PATH_MANUAL;
    }
    case SPN_INSTALL_OS_WINDOWS: {
      return facts->registry.kind == SPN_INSTALL_REG_OTHER ? SPN_INSTALL_PATH_MANUAL : SPN_INSTALL_PATH_UPDATED;
    }
  }
  return SPN_INSTALL_PATH_MANUAL;
}

static void push_action(spn_install_plan_t* plan, spn_install_action_t action) {
  SP_ASSERT(plan->num_path < SPN_INSTALL_MAX_PATH_ACTIONS);
  SP_ASSERT(action.role != SPN_INSTALL_ROLE_NONE);
  plan->path[plan->num_path++] = action;
}

static spn_install_action_t env_action(sp_mem_t mem, spn_install_layout_t* layout) {
  return (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_WRITE_FILE,
    .path = layout->env_file,
    .text = env_sh(mem, layout->root_expr),
    .role = SPN_INSTALL_ROLE_PATH,
  };
}

static spn_install_action_t fish_action(sp_mem_t mem, spn_install_layout_t* layout) {
  return (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_WRITE_FILE,
    .path = layout->fish_conf,
    .text = fish_sh(mem, layout->root_expr),
    .role = SPN_INSTALL_ROLE_FISH,
  };
}

static spn_install_action_t rc_action(sp_str_t path, sp_str_t text) {
  return (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_APPEND_LINE,
    .path = path,
    .text = text,
    .role = SPN_INSTALL_ROLE_RC,
  };
}

static spn_install_action_t github_action(sp_mem_t mem, spn_install_layout_t* layout) {
  return (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_APPEND_LINE,
    .path = layout->github_path,
    .text = sp_fmt(mem, "{}\n", sp_fmt_str(layout->bin_native)).value,
    .role = SPN_INSTALL_ROLE_PATH,
  };
}

static spn_install_action_t registry_action(sp_mem_t mem, spn_install_layout_t* layout, spn_install_facts_t* facts) {
  sp_str_t value = layout->bin_native;
  if (!sp_str_empty(facts->registry.path)) {
    value = sp_fmt(mem, "{};{}", sp_fmt_str(layout->bin_native), sp_fmt_str(facts->registry.path)).value;
  }
  return (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_SET_USER_PATH,
    .text = value,
    .reg = facts->registry.kind == SPN_INSTALL_REG_EXPAND ? SPN_INSTALL_REG_EXPAND : SPN_INSTALL_REG_SZ,
    .role = SPN_INSTALL_ROLE_PATH,
  };
}

spn_install_plan_t spn_install_plan(sp_mem_t mem, spn_install_layout_t* layout, spn_install_facts_t* facts) {
  spn_install_plan_t plan = sp_zero;

  if (!path_equal(layout->os, facts->exe, layout->exe)) {
    plan.install[plan.num_install++] = (spn_install_action_t) { .kind = SPN_INSTALL_ACTION_CREATE_DIR, .path = layout->bin };
    plan.install[plan.num_install++] = (spn_install_action_t) { .kind = SPN_INSTALL_ACTION_INSTALL_EXE, .path = layout->exe, .src = facts->exe };
  }

  plan.state = path_state(layout, facts);
  switch (plan.state) {
    case SPN_INSTALL_PATH_OK:
    case SPN_INSTALL_PATH_MANUAL: {
      break;
    }
    case SPN_INSTALL_PATH_CI: {
      switch (layout->os) {
        case SPN_INSTALL_OS_UNIX: {
          push_action(&plan, env_action(mem, layout));
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
          push_action(&plan, env_action(mem, layout));
          push_action(&plan, fish_action(mem, layout));
          sp_str_t rc_append = sp_fmt(mem, "\n{}\n", sp_fmt_str(layout->rc_line)).value;
          sp_for(it, layout->num_rc) {
            bool wanted = layout->rc[it].always || facts->rc[it].exists;
            if (wanted && !facts->rc[it].has_line) {
              push_action(&plan, rc_action(layout->rc[it].path, rc_append));
            }
          }
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

typedef struct {
  bool path;
  bool rc;
} stuck_t;

static stuck_t classify_stuck(spn_install_plan_t* plan, spn_install_result_t* result) {
  stuck_t stuck = sp_zero;
  sp_for(it, result->num_stuck) {
    switch (plan->path[result->stuck[it]].role) {
      case SPN_INSTALL_ROLE_NONE: break;
      case SPN_INSTALL_ROLE_PATH: stuck.path = true; break;
      case SPN_INSTALL_ROLE_RC: stuck.rc = true; break;
      case SPN_INSTALL_ROLE_FISH: break;
    }
  }
  return stuck;
}

static void push_msg(spn_install_msgs_t* msgs, spn_install_msg_t msg) {
  SP_ASSERT(msgs->count < SPN_INSTALL_MAX_MSGS);
  msgs->items[msgs->count++] = msg;
}

spn_install_msgs_t spn_install_report(spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_plan_t* plan, spn_install_result_t* result) {
  spn_install_msgs_t msgs = sp_zero;
  push_msg(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_INSTALLED, .subject = layout->exe });

  stuck_t stuck = classify_stuck(plan, result);
  if (plan->state == SPN_INSTALL_PATH_MANUAL || stuck.path) {
    push_msg(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_MANUAL, .subject = layout->bin_native });
  }
  else if (plan->state == SPN_INSTALL_PATH_UPDATED && !stuck.rc) {
    switch (layout->os) {
      case SPN_INSTALL_OS_UNIX: {
        push_msg(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_RESTART_SHELL, .subject = layout->rc_line });
        break;
      }
      case SPN_INSTALL_OS_WINDOWS: {
        push_msg(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_RESTART_TERMINAL });
        break;
      }
    }
  }

  sp_for(it, result->num_stuck) {
    spn_install_action_t* action = &plan->path[result->stuck[it]];
    if (action->kind == SPN_INSTALL_ACTION_SET_USER_PATH) {
      push_msg(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_STUCK_REGISTRY });
    }
    else {
      push_msg(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_STUCK_FILE, .subject = action->path });
    }
  }
  if (stuck.rc) {
    push_msg(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_ADD_LINE, .subject = layout->rc_line });
  }

  if (!sp_str_empty(facts->shadow) && !path_equal(layout->os, facts->shadow, layout->exe)) {
    push_msg(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_SHADOW, .subject = facts->shadow, .detail = layout->exe });
  }
  return msgs;
}
