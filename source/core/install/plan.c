#include "install/install.h"

#include "sp/str.h"

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

static spn_install_path_state_t path_state(spn_install_layout_t* layout) {
  if (layout->on_path) {
    return SPN_INSTALL_PATH_OK;
  }
  if (layout->no_modify_path) {
    return SPN_INSTALL_PATH_MANUAL;
  }
  if (!sp_str_empty(layout->github_path)) {
    return SPN_INSTALL_PATH_CI;
  }
  if (layout->os == SPN_INSTALL_OS_WINDOWS) {
    return SPN_INSTALL_PATH_UPDATED;
  }
  if (layout->num_rc) {
    return SPN_INSTALL_PATH_UPDATED;
  }
  return SPN_INSTALL_PATH_MANUAL;
}

static void push(spn_install_plan_t* plan, spn_install_action_t action) {
  SP_ASSERT(plan->num_path < SPN_INSTALL_MAX_PATH_ACTIONS);
  plan->path[plan->num_path++] = action;
}

static void plan_unix(sp_mem_t mem, spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_plan_t* plan) {
  sp_str_t rc_append = sp_fmt(mem, "\n{}\n", sp_fmt_str(layout->rc_line)).value;
  push(plan, (spn_install_action_t) { .kind = SPN_INSTALL_ACTION_WRITE_FILE, .path = layout->env_file, .text = env_sh(mem, layout->root_expr) });
  push(plan, (spn_install_action_t) { .kind = SPN_INSTALL_ACTION_WRITE_FILE, .path = layout->fish_conf, .text = fish_sh(mem, layout->root_expr) });
  sp_for(it, layout->num_rc) {
    bool wanted = layout->rc[it].always || facts->rc[it].exists;
    if (wanted && !facts->rc[it].has_line) {
      push(plan, (spn_install_action_t) { .kind = SPN_INSTALL_ACTION_APPEND_LINE, .path = layout->rc[it].path, .text = rc_append });
    }
  }
}

static void plan_windows(sp_mem_t mem, spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_plan_t* plan) {
  if (registry_contains(facts->registry.path, layout->bin_native)) {
    return;
  }
  sp_str_t value = layout->bin_native;
  if (!sp_str_empty(facts->registry.path)) {
    value = sp_fmt(mem, "{};{}", sp_fmt_str(layout->bin_native), sp_fmt_str(facts->registry.path)).value;
  }
  push(plan, (spn_install_action_t) { .kind = SPN_INSTALL_ACTION_SET_USER_PATH, .text = value, .expand = facts->registry.expand });
}

static void github(sp_mem_t mem, spn_install_layout_t* layout, spn_install_plan_t* plan) {
  push(plan, (spn_install_action_t) { .kind = SPN_INSTALL_ACTION_APPEND_LINE, .path = layout->github_path, .text = sp_fmt(mem, "{}\n", sp_fmt_str(layout->bin_native)).value });
}

static void plan_ci_unix(sp_mem_t mem, spn_install_layout_t* layout, spn_install_plan_t* plan) {
  push(plan, (spn_install_action_t) { .kind = SPN_INSTALL_ACTION_WRITE_FILE, .path = layout->env_file, .text = env_sh(mem, layout->root_expr) });
  github(mem, layout, plan);
}

spn_install_plan_t spn_install_plan(sp_mem_t mem, spn_install_layout_t* layout, spn_install_facts_t* facts) {
  spn_install_plan_t plan = sp_zero;

  if (!spn_install_path_equal(layout->os, facts->exe, layout->exe)) {
    plan.install[plan.num_install++] = (spn_install_action_t) { .kind = SPN_INSTALL_ACTION_CREATE_DIR, .path = layout->bin };
    plan.install[plan.num_install++] = (spn_install_action_t) { .kind = SPN_INSTALL_ACTION_INSTALL_EXE, .path = layout->exe, .src = facts->exe };
  }

  plan.state = path_state(layout);
  switch (plan.state) {
    case SPN_INSTALL_PATH_OK:
    case SPN_INSTALL_PATH_MANUAL: {
      break;
    }
    case SPN_INSTALL_PATH_CI: {
      switch (layout->os) {
        case SPN_INSTALL_OS_UNIX: plan_ci_unix(mem, layout, &plan); break;
        case SPN_INSTALL_OS_WINDOWS: github(mem, layout, &plan); break;
      }
      break;
    }
    case SPN_INSTALL_PATH_UPDATED: {
      switch (layout->os) {
        case SPN_INSTALL_OS_UNIX: plan_unix(mem, layout, facts, &plan); break;
        case SPN_INSTALL_OS_WINDOWS: plan_windows(mem, layout, facts, &plan); break;
      }
      break;
    }
  }
  return plan;
}
