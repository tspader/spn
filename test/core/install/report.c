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
  u32 stuck [4];
  u32 num_stuck;
  msg_spec_t expect [SPN_INSTALL_MAX_MSGS];
} test_t;

enum {
  UNIX_EXE,
  UNIX_ENV,
  UNIX_FISH,
  UNIX_PROFILE,
  UNIX_ZSHRC,
};

static const test_t tests [] = {
  {
    .name = "updated",
    .world = { INSTALL_WORLD_UNIX },
    .expect = {
      { SPN_INSTALL_MSG_RESTART_SHELL, RC_LINE },
    },
  },
  {
    .name = "on_path",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p:/h/.spn/bin" } },
      .exe = "/s/spn",
    },
  },
  {
    .name = "ci",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "GITHUB_PATH", "/gh" } },
      .exe = "/s/spn",
    },
  },
  {
    .name = "manual",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "SPN_INSTALL_NO_MODIFY_PATH", "1" } },
      .exe = "/s/spn",
    },
    .expect = {
      { SPN_INSTALL_MSG_MANUAL, "/h/.spn/bin" },
    },
  },
  {
    .name = "one_rc_stuck",
    .world = { INSTALL_WORLD_UNIX },
    .stuck = { UNIX_PROFILE },
    .num_stuck = 1,
    .expect = {
      { SPN_INSTALL_MSG_STUCK_APPEND, "/h/.profile", RC_LINE },
      { SPN_INSTALL_MSG_RESTART_SHELL, RC_LINE },
    },
  },
  {
    .name = "every_hook_stuck",
    .world = { INSTALL_WORLD_UNIX },
    .stuck = { UNIX_FISH, UNIX_PROFILE, UNIX_ZSHRC },
    .num_stuck = 3,
    .expect = {
      { SPN_INSTALL_MSG_STUCK_WRITE, "/h/.config/fish/conf.d/spn.fish" },
      { SPN_INSTALL_MSG_STUCK_APPEND, "/h/.profile", RC_LINE },
      { SPN_INSTALL_MSG_STUCK_APPEND, "/h/.zshrc", RC_LINE },
      { SPN_INSTALL_MSG_MANUAL, "/h/.spn/bin" },
    },
  },
  {
    .name = "env_stuck",
    .world = { INSTALL_WORLD_UNIX },
    .stuck = { UNIX_ENV },
    .num_stuck = 1,
    .expect = {
      { SPN_INSTALL_MSG_STUCK_WRITE, "/h/.spn/env" },
      { SPN_INSTALL_MSG_MANUAL, "/h/.spn/bin" },
    },
  },
  {
    .name = "fish_stuck",
    .world = { INSTALL_WORLD_UNIX },
    .stuck = { UNIX_FISH },
    .num_stuck = 1,
    .expect = {
      { SPN_INSTALL_MSG_STUCK_WRITE, "/h/.config/fish/conf.d/spn.fish" },
      { SPN_INSTALL_MSG_RESTART_SHELL, RC_LINE },
    },
  },
  {
    .name = "fish_stuck_rc_configured",
    .world = {
      INSTALL_WORLD_UNIX,
      .rc = {
        [INSTALL_RC_PROFILE] = { .exists = true, .has_line = true },
        [INSTALL_RC_ZSHRC] = { .exists = true, .has_line = true },
      },
    },
    .stuck = { 2 },
    .num_stuck = 1,
    .expect = {
      { SPN_INSTALL_MSG_STUCK_WRITE, "/h/.config/fish/conf.d/spn.fish" },
      { SPN_INSTALL_MSG_RESTART_SHELL, RC_LINE },
    },
  },
  {
    .name = "github_stuck",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "GITHUB_PATH", "/gh" } },
      .exe = "/s/spn",
    },
    .stuck = { 2 },
    .num_stuck = 1,
    .expect = {
      { SPN_INSTALL_MSG_STUCK_APPEND, "/gh", "/h/.spn/bin" },
      { SPN_INSTALL_MSG_MANUAL, "/h/.spn/bin" },
    },
  },
  {
    .name = "shadow",
    .world = {
      INSTALL_WORLD_UNIX,
      .shadow = "/p/spn",
    },
    .expect = {
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
      { SPN_INSTALL_MSG_RESTART_SHELL, RC_LINE },
    },
  },
  {
    .name = "windows",
    .world = { INSTALL_WORLD_WINDOWS },
    .expect = {
      { SPN_INSTALL_MSG_RESTART_TERMINAL },
    },
  },
  {
    .name = "windows_registry_stuck",
    .world = { INSTALL_WORLD_WINDOWS },
    .stuck = { 1 },
    .num_stuck = 1,
    .expect = {
      { SPN_INSTALL_MSG_STUCK_REGISTRY },
      { SPN_INSTALL_MSG_MANUAL, "C:\\u\\.spn\\bin" },
    },
  },
};

sp_test_each(install_report, messages, test_t, tests) {
  spn_install_layout_t layout = sp_zero;
  spn_install_facts_t facts = sp_zero;
  install_world_t world = it->world;
  sp_try(install_build(t, &world, &layout, &facts));

  spn_install_choices_t choices = spn_install_choices(&layout);
  spn_install_plan_t plan = spn_install_plan(sp_test_arena(t), &layout, &facts, &choices);

  spn_install_result_t result = sp_zero;
  sp_for(at, it->num_stuck) {
    sp_must_lt(t, it->stuck[at], plan.count);
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
