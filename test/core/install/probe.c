#include "harness.h"

#define RC_LINE ". \"$HOME/.spn/env\""

typedef struct {
  const c8* name;
  struct {
    u32 slot;
    const c8* content;
  } rc_files [2];
  const c8* spn_in [2];
  struct {
    spn_install_rc_state_t rc [SPN_INSTALL_MAX_RC];
    const c8* shadow;
  } expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "missing",
  },
  {
    .name = "exists",
    .rc_files = { { INSTALL_RC_PROFILE, "x\n" } },
    .expect = {
      .rc = { [INSTALL_RC_PROFILE] = { .exists = true } },
    },
  },
  {
    .name = "has_line",
    .rc_files = { { INSTALL_RC_PROFILE, RC_LINE "\n" } },
    .expect = {
      .rc = { [INSTALL_RC_PROFILE] = { .exists = true, .has_line = true } },
    },
  },
  {
    .name = "embedded_line",
    .rc_files = { { INSTALL_RC_PROFILE, "x " RC_LINE " y\n" } },
    .expect = {
      .rc = { [INSTALL_RC_PROFILE] = { .exists = true, .has_line = true } },
    },
  },
  {
    .name = "slots",
    .rc_files = { { INSTALL_RC_BASHRC, "x\n" }, { INSTALL_RC_ZSHRC, RC_LINE "\n" } },
    .expect = {
      .rc = {
        [INSTALL_RC_BASHRC] = { .exists = true },
        [INSTALL_RC_ZSHRC] = { .exists = true, .has_line = true },
      },
    },
  },
  {
    .name = "shadow_first",
    .spn_in = { "a", "b" },
    .expect = { .shadow = "a" },
  },
  {
    .name = "shadow_second",
    .spn_in = { "b" },
    .expect = { .shadow = "b" },
  },
};

sp_test_each(install_probe, facts, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t dir = sp_test_dir(t);

  sp_str_t home = sp_fs_join_path(mem, dir, sp_str_lit("home"));
  sp_str_t a = sp_fs_join_path(mem, dir, sp_str_lit("a"));
  sp_str_t b = sp_fs_join_path(mem, dir, sp_str_lit("b"));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(home));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(a));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(b));

  sp_env_t env = sp_zero;
  sp_env_init(mem, &env);
  sp_env_insert(&env, sp_str_lit("HOME"), home);
  sp_env_insert(&env, sp_str_lit("PATH"), sp_fmt(mem, "{}:{}", sp_fmt_str(a), sp_fmt_str(b)).value);

  spn_install_layout_t layout = spn_install_resolve(mem, SPN_INSTALL_OS_UNIX, &env);
  sp_must_eq(t, (u32)SPN_INSTALL_OK, (u32)layout.err);

  sp_carr_for(it->rc_files, at) {
    if (!it->rc_files[at].content) {
      break;
    }
    sp_must_lt(t, it->rc_files[at].slot, layout.num_rc);
    sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_file_cstr(layout.rc[it->rc_files[at].slot].path, it->rc_files[at].content));
  }
  sp_carr_for(it->spn_in, at) {
    if (!it->spn_in[at]) {
      break;
    }
    sp_str_t path = sp_fs_join_path(mem, sp_fs_join_path(mem, dir, sp_cstr_as_str(it->spn_in[at])), sp_str_lit("spn"));
    sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_file_cstr(path, "S"));
  }

  spn_install_facts_t facts = spn_install_probe(mem, &layout);

  sp_for(slot, layout.num_rc) {
    sp_expect_eq(t, it->expect.rc[slot].exists, facts.rc[slot].exists);
    sp_expect_eq(t, it->expect.rc[slot].has_line, facts.rc[slot].has_line);
  }
  sp_str_t shadow = sp_zero;
  if (it->expect.shadow) {
    shadow = sp_fs_join_path(mem, sp_fs_join_path(mem, dir, sp_cstr_as_str(it->expect.shadow)), sp_str_lit("spn"));
  }
  sp_expect_str_eq(t, facts.shadow, shadow);
  return SP_OK;
}

sp_test(install_probe, exe) {
  sp_mem_t mem = sp_test_arena(t);

  sp_env_t env = sp_zero;
  sp_env_init(mem, &env);
  sp_env_insert(&env, sp_str_lit("HOME"), sp_test_dir(t));
  sp_env_insert(&env, sp_str_lit("USERPROFILE"), sp_test_dir(t));

  spn_install_layout_t layout = spn_install_resolve(mem, spn_install_os_host(), &env);
  sp_must_eq(t, (u32)SPN_INSTALL_OK, (u32)layout.err);

  spn_install_facts_t facts = spn_install_probe(mem, &layout);
  sp_must(t, !sp_str_empty(facts.exe));
  sp_expect(t, sp_fs_is_absolute(facts.exe));
  sp_expect(t, sp_fs_is_file(facts.exe));
  return SP_OK;
}
