#include "harness.h"

#define RC_LINE ". \"$HOME/.spn/env\""

typedef struct {
  spn_install_msg_kind_t kind;
  const c8* subject;
  const c8* detail;
} msg_spec_t;

typedef struct {
  const c8* name;
  install_world_t world;
  bool fatal;
  u32 stuck [2];
  u32 num_stuck;
  msg_spec_t expect [SPN_INSTALL_MAX_MSGS];
} test_t;

static const test_t tests [] = {
  {
    .name = "updated",
    .world = { INSTALL_WORLD_UNIX },
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "/h/.spn/bin/spn" },
      { SPN_INSTALL_MSG_RESTART_SHELL, RC_LINE },
    },
  },
  {
    .name = "on_path",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p:/h/.spn/bin" } },
      .exe = "/s/spn",
    },
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "/h/.spn/bin/spn" },
    },
  },
  {
    .name = "ci",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "GITHUB_PATH", "/gh" } },
      .exe = "/s/spn",
    },
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "/h/.spn/bin/spn" },
    },
  },
  {
    .name = "manual",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "SPN_INSTALL_NO_MODIFY_PATH", "1" } },
      .exe = "/s/spn",
    },
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "/h/.spn/bin/spn" },
      { SPN_INSTALL_MSG_MANUAL, "/h/.spn/bin" },
    },
  },
  {
    .name = "stuck_rc",
    .world = { INSTALL_WORLD_UNIX },
    .stuck = { 2 },
    .num_stuck = 1,
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "/h/.spn/bin/spn" },
      { SPN_INSTALL_MSG_STUCK_FILE, "/h/.profile" },
      { SPN_INSTALL_MSG_ADD_LINE, RC_LINE },
    },
  },
  {
    .name = "stuck_two_rc",
    .world = { INSTALL_WORLD_UNIX },
    .stuck = { 2, 3 },
    .num_stuck = 2,
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "/h/.spn/bin/spn" },
      { SPN_INSTALL_MSG_STUCK_FILE, "/h/.profile" },
      { SPN_INSTALL_MSG_STUCK_FILE, "/h/.zshrc" },
      { SPN_INSTALL_MSG_ADD_LINE, RC_LINE },
    },
  },
  {
    .name = "stuck_env",
    .world = { INSTALL_WORLD_UNIX },
    .stuck = { 0 },
    .num_stuck = 1,
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "/h/.spn/bin/spn" },
      { SPN_INSTALL_MSG_MANUAL, "/h/.spn/bin" },
      { SPN_INSTALL_MSG_STUCK_FILE, "/h/.spn/env" },
    },
  },
  {
    .name = "stuck_fish",
    .world = { INSTALL_WORLD_UNIX },
    .stuck = { 1 },
    .num_stuck = 1,
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "/h/.spn/bin/spn" },
      { SPN_INSTALL_MSG_RESTART_SHELL, RC_LINE },
      { SPN_INSTALL_MSG_STUCK_FILE, "/h/.config/fish/conf.d/spn.fish" },
    },
  },
  {
    .name = "stuck_github",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "GITHUB_PATH", "/gh" } },
      .exe = "/s/spn",
    },
    .stuck = { 1 },
    .num_stuck = 1,
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "/h/.spn/bin/spn" },
      { SPN_INSTALL_MSG_MANUAL, "/h/.spn/bin" },
      { SPN_INSTALL_MSG_STUCK_FILE, "/gh" },
    },
  },
  {
    .name = "shadow",
    .world = {
      INSTALL_WORLD_UNIX,
      .shadow = "/p/spn",
    },
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "/h/.spn/bin/spn" },
      { SPN_INSTALL_MSG_RESTART_SHELL, RC_LINE },
      { SPN_INSTALL_MSG_SHADOW, "/p/spn", "/h/.spn/bin/spn" },
    },
  },
  {
    .name = "shadow_self",
    .world = {
      INSTALL_WORLD_UNIX,
      .shadow = "/h/.spn/bin/spn",
    },
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "/h/.spn/bin/spn" },
      { SPN_INSTALL_MSG_RESTART_SHELL, RC_LINE },
    },
  },
  {
    .name = "windows",
    .world = { INSTALL_WORLD_WINDOWS },
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "C:/u/.spn/bin/spn.exe" },
      { SPN_INSTALL_MSG_RESTART_TERMINAL },
    },
  },
  {
    .name = "windows_stuck_registry",
    .world = { INSTALL_WORLD_WINDOWS },
    .stuck = { 0 },
    .num_stuck = 1,
    .expect = {
      { SPN_INSTALL_MSG_INSTALLED, "C:/u/.spn/bin/spn.exe" },
      { SPN_INSTALL_MSG_MANUAL, "C:\\u\\.spn\\bin" },
      { SPN_INSTALL_MSG_STUCK_REGISTRY },
    },
  },
  {
    .name = "fatal",
    .world = { INSTALL_WORLD_UNIX },
    .fatal = true,
  },
};

sp_test_each(install_report, messages, test_t, tests) {
  spn_install_layout_t layout = sp_zero;
  spn_install_facts_t facts = sp_zero;
  install_world_t world = it->world;
  sp_try(install_build(t, &world, &layout, &facts));

  spn_install_plan_t plan = spn_install_plan(sp_test_arena(t), &layout, &facts);

  spn_install_result_t result = sp_zero;
  if (it->fatal) {
    result.err = SP_ERR;
    result.failed = plan.install[0];
  }
  sp_for(at, it->num_stuck) {
    sp_must_lt(t, it->stuck[at], plan.num_path);
    result.stuck[result.num_stuck++] = it->stuck[at];
  }

  spn_install_msgs_t msgs = spn_install_report(&layout, &facts, &plan, &result);

  u32 num_expect = 0;
  sp_carr_detect_len(it->expect, num_expect, it->expect[num_expect].kind);
  sp_must_eq(t, num_expect, msgs.count);
  sp_for(at, msgs.count) {
    sp_expect_eq(t, (u32)it->expect[at].kind, (u32)msgs.items[at].kind);
    sp_expect_str_eq_c(t, msgs.items[at].subject, it->expect[at].subject ? it->expect[at].subject : "");
    sp_expect_str_eq_c(t, msgs.items[at].detail, it->expect[at].detail ? it->expect[at].detail : "");
  }
  return SP_OK;
}
