#include "harness.h"

#if !defined(SP_WIN32)
  #include <unistd.h>
#endif

typedef struct {
  const c8* path;
  const c8* text;
  bool exec;
} file_spec_t;

typedef struct {
  const c8* name;
  bool unix_only;
  file_spec_t pre [2];
  const c8* ro_dir;
  install_action_spec_t install [SPN_INSTALL_MAX_INSTALL_ACTIONS + 1];
  install_action_spec_t path [3];
  struct {
    bool fatal;
    spn_install_action_kind_t failed_kind;
    u32 stuck [2];
    u32 num_stuck;
    file_spec_t files [2];
    const c8* executable;
    const c8* dirs [2];
    const c8* absent [2];
  } expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "create_dir",
    .install = {
      { SPN_INSTALL_ACTION_CREATE_DIR, .path = "b/c" },
    },
    .expect = {
      .dirs = { "b/c" },
    },
  },
  {
    .name = "install_exe",
    .pre = { { "s/spn", "X", .exec = true } },
    .install = {
      { SPN_INSTALL_ACTION_CREATE_DIR, .path = "bin" },
      { SPN_INSTALL_ACTION_INSTALL_EXE, .path = "bin/spn", .src = "s/spn" },
    },
    .expect = {
      .files = { { "bin/spn", "X" }, { "s/spn", "X" } },
      .executable = "bin/spn",
    },
  },
  {
    .name = "install_replaces",
    .pre = { { "s/spn", "NEW", .exec = true }, { "bin/spn", "OLD" } },
    .install = {
      { SPN_INSTALL_ACTION_INSTALL_EXE, .path = "bin/spn", .src = "s/spn" },
    },
    .expect = {
      .files = { { "bin/spn", "NEW" } },
      .executable = "bin/spn",
    },
  },
  {
    .name = "write_file",
    .path = {
      { SPN_INSTALL_ACTION_WRITE_FILE, .path = "c/f/spn.fish", .text = "F\n" },
    },
    .expect = {
      .files = { { "c/f/spn.fish", "F\n" } },
    },
  },
  {
    .name = "write_replaces",
    .pre = { { "env", "OLD" } },
    .path = {
      { SPN_INSTALL_ACTION_WRITE_FILE, .path = "env", .text = "NEW" },
    },
    .expect = {
      .files = { { "env", "NEW" } },
    },
  },
  {
    .name = "append_creates",
    .path = {
      { SPN_INSTALL_ACTION_APPEND_LINE, .path = "rc", .text = "\nL\n" },
    },
    .expect = {
      .files = { { "rc", "\nL\n" } },
    },
  },
  {
    .name = "append_appends",
    .pre = { { "rc", "A\n" } },
    .path = {
      { SPN_INSTALL_ACTION_APPEND_LINE, .path = "rc", .text = "\nL\n" },
    },
    .expect = {
      .files = { { "rc", "A\n\nL\n" } },
    },
  },
  {
    .name = "fatal_stops",
    .pre = { { "x", "F" } },
    .install = {
      { SPN_INSTALL_ACTION_CREATE_DIR, .path = "x" },
      { SPN_INSTALL_ACTION_INSTALL_EXE, .path = "bin/spn", .src = "s/spn" },
    },
    .expect = {
      .fatal = true,
      .failed_kind = SPN_INSTALL_ACTION_CREATE_DIR,
      .absent = { "bin/spn" },
    },
  },
  {
    .name = "fatal_missing_source",
    .install = {
      { SPN_INSTALL_ACTION_CREATE_DIR, .path = "bin" },
      { SPN_INSTALL_ACTION_INSTALL_EXE, .path = "bin/spn", .src = "s/absent" },
    },
    .expect = {
      .fatal = true,
      .failed_kind = SPN_INSTALL_ACTION_INSTALL_EXE,
      .absent = { "bin/spn" },
    },
  },
  {
    .name = "stuck_continues",
    .unix_only = true,
    .ro_dir = "ro",
    .path = {
      { SPN_INSTALL_ACTION_APPEND_LINE, .path = "ro/rc", .text = "L" },
      { SPN_INSTALL_ACTION_APPEND_LINE, .path = "ok", .text = "L" },
    },
    .expect = {
      .stuck = { 0 },
      .num_stuck = 1,
      .files = { { "ok", "L" } },
      .absent = { "ro/rc" },
    },
  },
};

static bool is_executable(sp_str_t path) {
#if defined(SP_WIN32)
  return sp_fs_is_file(path);
#else
  sp_sys_file_meta_t meta = sp_zero;
  if (sp_sys_get_path_metadata_s(sp_sys_get_root(0), path, &meta)) {
    return false;
  }
  return (meta.raw_attrs & 0111) != 0;
#endif
}

static sp_err_t chmod_exec(sp_test_t* t, sp_mem_t mem, sp_str_t path) {
  sp_ps_output_t chmod = sp_ps_run(mem, (sp_ps_config_t) {
    .command = sp_str_lit("chmod"),
    .args = { sp_str_lit("+x"), path },
  });
  sp_must_eq(t, 0, chmod.status.exit_code);
  return SP_OK;
}

static u32 build_actions(sp_test_t* t, const install_action_spec_t* specs, u32 max, spn_install_action_t* out) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t dir = sp_test_dir(t);
  u32 count = 0;
  while (count < max && specs[count].kind) {
    count++;
  }
  sp_for(at, count) {
    out[at] = (spn_install_action_t) {
      .kind = specs[at].kind,
      .path = sp_fs_join_path(mem, dir, sp_cstr_as_str(specs[at].path)),
      .src = specs[at].src ? sp_fs_join_path(mem, dir, sp_cstr_as_str(specs[at].src)) : sp_zero_s(sp_str_t),
      .text = sp_cstr_as_str(specs[at].text ? specs[at].text : ""),
    };
  }
  return count;
}

sp_test_each(install_exec, actions, test_t, tests) {
  if (it->unix_only) {
    sp_test_skip_on_win32();
  }
#if !defined(SP_WIN32)
  if (it->ro_dir && geteuid() == 0) {
    return sp_test_skip(t, "read-only fixtures do not bind as root");
  }
#endif

  sp_mem_t mem = sp_test_arena(t);
  sp_str_t dir = sp_test_dir(t);

  sp_carr_for(it->pre, at) {
    if (!it->pre[at].path) {
      break;
    }
    sp_str_t path = sp_fs_join_path(mem, dir, sp_cstr_as_str(it->pre[at].path));
    sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(sp_fs_parent_path(path)));
    sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_file_cstr(path, it->pre[at].text));
    if (it->pre[at].exec) {
      sp_try(chmod_exec(t, mem, path));
    }
  }
  if (it->ro_dir) {
    sp_str_t path = sp_fs_join_path(mem, dir, sp_cstr_as_str(it->ro_dir));
    sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(path));
    sp_ps_output_t chmod = sp_ps_run(mem, (sp_ps_config_t) {
      .command = sp_str_lit("chmod"),
      .args = { sp_str_lit("500"), path },
    });
    sp_must_eq(t, 0, chmod.status.exit_code);
  }

  spn_install_plan_t plan = sp_zero;
  plan.num_install = build_actions(t, it->install, SPN_INSTALL_MAX_INSTALL_ACTIONS, plan.install);
  plan.num_path = build_actions(t, it->path, SPN_INSTALL_MAX_PATH_ACTIONS, plan.path);

  spn_install_result_t result = spn_install_execute(&plan);

  if (it->ro_dir) {
    sp_ps_run(mem, (sp_ps_config_t) {
      .command = sp_str_lit("chmod"),
      .args = { sp_str_lit("755"), sp_fs_join_path(mem, dir, sp_cstr_as_str(it->ro_dir)) },
    });
  }

  sp_must_eq(t, it->expect.fatal, result.err != SP_OK);
  if (it->expect.fatal) {
    sp_expect_eq(t, (u32)it->expect.failed_kind, (u32)result.failed.kind);
  }
  sp_must_eq(t, it->expect.num_stuck, result.num_stuck);
  sp_for(at, it->expect.num_stuck) {
    sp_expect_eq(t, it->expect.stuck[at], result.stuck[at]);
  }

  sp_carr_for(it->expect.files, at) {
    if (!it->expect.files[at].path) {
      break;
    }
    sp_str_t path = sp_fs_join_path(mem, dir, sp_cstr_as_str(it->expect.files[at].path));
    sp_str_t content = sp_zero;
    sp_must_eq(t, (u32)SP_OK, (u32)sp_io_read_file(mem, path, &content));
    sp_expect_str_eq_c(t, content, it->expect.files[at].text);
  }
  if (it->expect.executable) {
    sp_expect(t, is_executable(sp_fs_join_path(mem, dir, sp_cstr_as_str(it->expect.executable))));
  }
  sp_carr_for(it->expect.dirs, at) {
    if (!it->expect.dirs[at]) {
      break;
    }
    sp_expect(t, sp_fs_is_dir(sp_fs_join_path(mem, dir, sp_cstr_as_str(it->expect.dirs[at]))));
  }
  sp_carr_for(it->expect.absent, at) {
    if (!it->expect.absent[at]) {
      break;
    }
    sp_expect(t, !sp_fs_exists(sp_fs_join_path(mem, dir, sp_cstr_as_str(it->expect.absent[at]))));
  }
  return SP_OK;
}

sp_test(install_exec, busy_replace) {
  sp_test_skip_on_win32();

  sp_mem_t mem = sp_test_arena(t);
  sp_str_t dir = sp_test_dir(t);

  sp_str_t sleep_bin = sp_zero;
  {
    sp_ps_output_t which = sp_ps_run(mem, (sp_ps_config_t) {
      .command = sp_str_lit("sh"),
      .args = { sp_str_lit("-c"), sp_str_lit("command -v sleep") },
    });
    sp_must_eq(t, 0, which.status.exit_code);
    sleep_bin = sp_str_trim_right(which.out);
  }

  sp_str_t target = sp_fs_join_path(mem, dir, sp_str_lit("bin/spn"));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(sp_fs_parent_path(target)));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_copy_file(sleep_bin, target));

  sp_ps_t running = sp_ps_create(mem, (sp_ps_config_t) {
    .command = target,
    .args = { sp_str_lit("30") },
    .io = SP_PS_NO_STDIO,
  });
  sp_must(t, running.os);

  sp_str_t source = sp_fs_join_path(mem, dir, sp_str_lit("s/spn"));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(sp_fs_parent_path(source)));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_file_cstr(source, "N"));

  spn_install_plan_t plan = sp_zero;
  plan.install[plan.num_install++] = (spn_install_action_t) {
    .kind = SPN_INSTALL_ACTION_INSTALL_EXE,
    .path = target,
    .src = source,
  };
  spn_install_result_t result = spn_install_execute(&plan);

  sp_ps_kill(&running);
  sp_ps_wait(&running);
  sp_ps_free(&running);

  sp_must_eq(t, (u32)SP_OK, (u32)result.err);
  sp_str_t content = sp_zero;
  sp_must_eq(t, (u32)SP_OK, (u32)sp_io_read_file(mem, target, &content));
  sp_expect_str_eq_c(t, content, "N");
  return SP_OK;
}
