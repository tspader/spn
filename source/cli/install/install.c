#include "install/install.h"

#include "install/plan.h"
#include "atomic_file/atomic_file.h"
#include "fs/fs.h"

static spn_install_os_t os_host() {
#if defined(SP_WIN32)
  return SPN_INSTALL_OS_WINDOWS;
#else
  return SPN_INSTALL_OS_UNIX;
#endif
}

#if defined(SP_WIN32)
static spn_install_reg_t probe_registry_value(sp_mem_t mem, HKEY key, DWORD type, DWORD size, sp_str_t* out) {
  spn_install_reg_t kind = SPN_INSTALL_REG_NONE;
  switch (type) {
    case REG_SZ: kind = SPN_INSTALL_REG_SZ; break;
    case REG_EXPAND_SZ: kind = SPN_INSTALL_REG_EXPAND; break;
    default: return SPN_INSTALL_REG_OTHER;
  }
  if (size < sizeof(u16)) {
    return kind;
  }

  u16* buffer = (u16*)sp_mem_allocator_alloc(mem, size);
  if (RegQueryValueExW(key, L"Path", SP_NULLPTR, &type, (BYTE*)buffer, &size) != ERROR_SUCCESS) {
    return SPN_INSTALL_REG_OTHER;
  }
  u32 len = (u32)(size / sizeof(u16));
  while (len && !buffer[len - 1]) {
    len--;
  }
  sp_str_t value = sp_zero;
  if (sp_wtf16_to_wtf8(mem, (sp_wide_str_t) { .data = buffer, .len = len }, &value)) {
    return SPN_INSTALL_REG_OTHER;
  }
  *out = value;
  return kind;
}

static void probe_registry(sp_mem_t mem, spn_install_facts_t* facts) {
  HKEY key = SP_NULLPTR;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_QUERY_VALUE, &key)) {
    return;
  }

  DWORD type = 0;
  DWORD size = 0;
  if (RegQueryValueExW(key, L"Path", SP_NULLPTR, &type, SP_NULLPTR, &size) == ERROR_SUCCESS) {
    facts->registry.kind = probe_registry_value(mem, key, type, size, &facts->registry.path);
  }
  RegCloseKey(key);
}

static sp_err_t set_user_path(spn_install_action_t* action) {
  sp_err_t err = SP_ERR_SYS;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_wide_str_t value = sp_zero;
  sp_try_goto(sp_wtf8_to_wtf16(scratch.mem, action->text, &value), err, done);

  HKEY key = SP_NULLPTR;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &key)) {
    goto done;
  }
  DWORD kind = action->reg == SPN_INSTALL_REG_EXPAND ? REG_EXPAND_SZ : REG_SZ;
  if (!RegSetValueExW(key, L"Path", 0, kind, (const BYTE*)value.data, (DWORD)((value.len + 1) * sizeof(u16)))) {
    err = SP_OK;
  }
  RegCloseKey(key);
  if (!err) {
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, SP_NULLPTR);
  }

done:
  sp_mem_end_scratch(scratch);
  return err;
}
#else
static sp_err_t set_user_path(spn_install_action_t* action) {
  SP_UNUSED(action);
  SP_UNREACHABLE_RETURN(SP_ERR);
}
#endif

static spn_install_facts_t probe_facts(sp_mem_t mem, spn_install_layout_t* layout) {
  spn_install_facts_t facts = sp_zero;
  facts.exe = sp_fs_get_exe_path(mem);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(mem);
  sp_for(it, layout->num_rc) {
    facts.rc[it].exists = sp_fs_is_file(layout->rc[it].path);
    if (facts.rc[it].exists) {
      sp_str_t content = sp_zero;
      if (!sp_io_read_file(scratch.mem, layout->rc[it].path, &content)) {
        facts.rc[it].has_line = sp_str_contains(content, sp_str_lit(SPN_INSTALL_RC_LINE));
      }
    }
  }
  sp_mem_end_scratch(scratch);

  facts.fish = sp_fs_is_dir(layout->fish_dir);

  sp_da_for(layout->shadows, it) {
    if (sp_fs_is_target_file(layout->shadows[it])) {
      facts.shadow = layout->shadows[it];
      break;
    }
  }

#if defined(SP_WIN32)
  probe_registry(mem, &facts);
#endif
  return facts;
}

static sp_err_t apply(spn_install_action_t* action) {
  switch (action->kind) {
    case SPN_INSTALL_ACTION_NONE: return SP_ERR;
    case SPN_INSTALL_ACTION_INSTALL_EXE: return sp_fs_copy_atomic(action->path, action->src);
    case SPN_INSTALL_ACTION_WRITE_FILE: return sp_fs_write_atomic(action->path, action->text);
    case SPN_INSTALL_ACTION_APPEND_LINE: return sp_fs_append(action->path, action->text);
    case SPN_INSTALL_ACTION_SET_USER_PATH: return set_user_path(action);
  }
  return SP_ERR;
}

static spn_install_result_t execute(spn_install_plan_t* plan) {
  spn_install_result_t result = sp_zero;

  sp_for(it, plan->count) {
    sp_err_t err = apply(&plan->actions[it]);
    if (!err) {
      continue;
    }
    if (plan->actions[it].role == SPN_INSTALL_ROLE_EXE) {
      result.err = err;
      result.failed = plan->actions[it];
      return result;
    }
    result.stuck[result.num_stuck++] = (u32)it;
  }
  return result;
}

spn_install_probe_t spn_install_probe(sp_mem_t mem) {
  spn_install_probe_t probe = sp_zero;
  sp_env_t env = sp_env_capture(mem);
  probe.layout = spn_install_resolve(mem, os_host(), &env);
  if (probe.layout.err) {
    return probe;
  }
  probe.facts = probe_facts(mem, &probe.layout);
  return probe;
}

spn_install_t spn_install_execute(spn_install_probe_t* probe, spn_install_plan_t* plan) {
  spn_install_result_t result = execute(plan);
  if (result.err) {
    return (spn_install_t) { .err = SPN_INSTALL_ERR_EXE, .failed = result.failed };
  }
  return (spn_install_t) {
    .err = result.num_stuck ? SPN_INSTALL_ERR_STUCK : SPN_INSTALL_OK,
    .exe = probe->layout.exe,
    .changes = plan->count,
    .stuck = result.num_stuck,
    .msgs = spn_install_report(&probe->layout, &probe->facts, plan, &result),
  };
}

spn_install_t spn_install(sp_mem_t mem) {
  spn_install_probe_t probe = spn_install_probe(mem);
  if (probe.layout.err) {
    return (spn_install_t) { .err = probe.layout.err };
  }

  spn_install_choices_t choices = spn_install_choices(&probe.layout, &probe.facts);
  spn_install_plan_t plan = spn_install_plan(mem, &probe.layout, &probe.facts, &choices);
  return spn_install_execute(&probe, &plan);
}
