#include "install/install.h"

#include "str/str.h"

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

  if (!spn_install_path_equal(layout->os, facts->exe, layout->exe)) {
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
