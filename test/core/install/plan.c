#include "harness.h"

#define ENV_SH \
  "case \":${PATH}:\" in\n" \
  "  *:\"$HOME/.spn/bin\":*) ;;\n" \
  "  *) export PATH=\"$HOME/.spn/bin:${PATH}\" ;;\n" \
  "esac\n"

#define FISH_SH \
  "if not contains \"$HOME/.spn/bin\" $PATH\n" \
  "  set --export PATH \"$HOME/.spn/bin\" $PATH\n" \
  "end\n"

#define CUSTOM_ENV_SH \
  "case \":${PATH}:\" in\n" \
  "  *:\"/o/bin\":*) ;;\n" \
  "  *) export PATH=\"/o/bin:${PATH}\" ;;\n" \
  "esac\n"

#define CUSTOM_FISH_SH \
  "if not contains \"/o/bin\" $PATH\n" \
  "  set --export PATH \"/o/bin\" $PATH\n" \
  "end\n"

#define RC_APPEND "\n. \"$HOME/.spn/env\"\n"
#define CUSTOM_RC_APPEND "\n. \"/o/env\"\n"

#define WIN_EXE "C:/s/spn.exe"
#define WIN_BIN "C:\\u\\.spn\\bin"

typedef struct {
  const c8* name;
  install_world_t world;
  struct {
    spn_install_path_state_t state;
    install_action_spec_t install [SPN_INSTALL_MAX_INSTALL_ACTIONS + 1];
    install_action_spec_t path [SPN_INSTALL_MAX_PATH_ACTIONS + 1];
  } expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "fresh",
    .world = { INSTALL_WORLD_UNIX },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "/h/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "/h/.spn/bin/spn", .src = "/s/spn" },
      },
      .path = {
        { SPN_INSTALL_ACTION_WRITE_FILE, "/h/.spn/env", .text = ENV_SH },
        { SPN_INSTALL_ACTION_WRITE_FILE, "/h/.config/fish/conf.d/spn.fish", .text = FISH_SH },
        { SPN_INSTALL_ACTION_APPEND_LINE, "/h/.profile", .text = RC_APPEND },
        { SPN_INSTALL_ACTION_APPEND_LINE, "/h/.zshrc", .text = RC_APPEND },
      },
    },
  },
  {
    .name = "custom",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "SPN_INSTALL", "/o" } },
      .exe = "/s/spn",
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "/o/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "/o/bin/spn", .src = "/s/spn" },
      },
      .path = {
        { SPN_INSTALL_ACTION_WRITE_FILE, "/o/env", .text = CUSTOM_ENV_SH },
        { SPN_INSTALL_ACTION_WRITE_FILE, "/h/.config/fish/conf.d/spn.fish", .text = CUSTOM_FISH_SH },
        { SPN_INSTALL_ACTION_APPEND_LINE, "/h/.profile", .text = CUSTOM_RC_APPEND },
        { SPN_INSTALL_ACTION_APPEND_LINE, "/h/.zshrc", .text = CUSTOM_RC_APPEND },
      },
    },
  },
  {
    .name = "rc_exists",
    .world = {
      INSTALL_WORLD_UNIX,
      .rc = {
        [INSTALL_RC_PROFILE] = { .exists = true },
        [INSTALL_RC_BASHRC] = { .exists = true },
      },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "/h/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "/h/.spn/bin/spn", .src = "/s/spn" },
      },
      .path = {
        { SPN_INSTALL_ACTION_WRITE_FILE, "/h/.spn/env", .text = ENV_SH },
        { SPN_INSTALL_ACTION_WRITE_FILE, "/h/.config/fish/conf.d/spn.fish", .text = FISH_SH },
        { SPN_INSTALL_ACTION_APPEND_LINE, "/h/.profile", .text = RC_APPEND },
        { SPN_INSTALL_ACTION_APPEND_LINE, "/h/.bashrc", .text = RC_APPEND },
        { SPN_INSTALL_ACTION_APPEND_LINE, "/h/.zshrc", .text = RC_APPEND },
      },
    },
  },
  {
    .name = "has_line",
    .world = {
      INSTALL_WORLD_UNIX,
      .rc = {
        [INSTALL_RC_PROFILE] = { .exists = true, .has_line = true },
      },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "/h/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "/h/.spn/bin/spn", .src = "/s/spn" },
      },
      .path = {
        { SPN_INSTALL_ACTION_WRITE_FILE, "/h/.spn/env", .text = ENV_SH },
        { SPN_INSTALL_ACTION_WRITE_FILE, "/h/.config/fish/conf.d/spn.fish", .text = FISH_SH },
        { SPN_INSTALL_ACTION_APPEND_LINE, "/h/.zshrc", .text = RC_APPEND },
      },
    },
  },
  {
    .name = "on_path",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p:/h/.spn/bin" } },
      .exe = "/s/spn",
    },
    .expect = {
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "/h/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "/h/.spn/bin/spn", .src = "/s/spn" },
      },
    },
  },
  {
    .name = "no_modify",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "SPN_INSTALL_NO_MODIFY_PATH", "1" } },
      .exe = "/s/spn",
    },
    .expect = {
      .state = SPN_INSTALL_PATH_MANUAL,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "/h/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "/h/.spn/bin/spn", .src = "/s/spn" },
      },
    },
  },
  {
    .name = "no_modify_beats_ci",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "SPN_INSTALL_NO_MODIFY_PATH", "1" }, { "GITHUB_PATH", "/gh" } },
      .exe = "/s/spn",
    },
    .expect = {
      .state = SPN_INSTALL_PATH_MANUAL,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "/h/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "/h/.spn/bin/spn", .src = "/s/spn" },
      },
    },
  },
  {
    .name = "on_path_beats_ci",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p:/h/.spn/bin" }, { "GITHUB_PATH", "/gh" } },
      .exe = "/s/spn",
    },
    .expect = {
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "/h/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "/h/.spn/bin/spn", .src = "/s/spn" },
      },
    },
  },
  {
    .name = "ci",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "GITHUB_PATH", "/gh" } },
      .exe = "/s/spn",
    },
    .expect = {
      .state = SPN_INSTALL_PATH_CI,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "/h/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "/h/.spn/bin/spn", .src = "/s/spn" },
      },
      .path = {
        { SPN_INSTALL_ACTION_WRITE_FILE, "/h/.spn/env", .text = ENV_SH },
        { SPN_INSTALL_ACTION_APPEND_LINE, "/gh", .text = "/h/.spn/bin\n" },
      },
    },
  },
  {
    .name = "ci_windows",
    .world = {
      .vars = { { "USERPROFILE", "C:\\u" }, { "PATH", "C:\\p" }, { "GITHUB_PATH", "C:\\gh" } },
      .os = SPN_INSTALL_OS_WINDOWS,
      .exe = WIN_EXE,
    },
    .expect = {
      .state = SPN_INSTALL_PATH_CI,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "C:/u/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "C:/u/.spn/bin/spn.exe", .src = WIN_EXE },
      },
      .path = {
        { SPN_INSTALL_ACTION_APPEND_LINE, "C:/gh", .text = WIN_BIN "\n" },
      },
    },
  },
  {
    .name = "manual_no_home",
    .world = {
      .vars = { { "SPN_INSTALL", "/o" }, { "PATH", "/p" } },
      .exe = "/s/spn",
    },
    .expect = {
      .state = SPN_INSTALL_PATH_MANUAL,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "/o/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "/o/bin/spn", .src = "/s/spn" },
      },
    },
  },
  {
    .name = "windows_fresh",
    .world = { INSTALL_WORLD_WINDOWS },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "C:/u/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "C:/u/.spn/bin/spn.exe", .src = WIN_EXE },
      },
      .path = {
        { SPN_INSTALL_ACTION_SET_USER_PATH, .text = WIN_BIN },
      },
    },
  },
  {
    .name = "windows_prepend",
    .world = {
      INSTALL_WORLD_WINDOWS,
      .registry = { .path = "C:\\old" },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "C:/u/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "C:/u/.spn/bin/spn.exe", .src = WIN_EXE },
      },
      .path = {
        { SPN_INSTALL_ACTION_SET_USER_PATH, .text = WIN_BIN ";C:\\old" },
      },
    },
  },
  {
    .name = "windows_registry_present",
    .world = {
      INSTALL_WORLD_WINDOWS,
      .registry = { .path = "c:\\U\\.spn\\BIN;C:\\old" },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "C:/u/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "C:/u/.spn/bin/spn.exe", .src = WIN_EXE },
      },
    },
  },
  {
    .name = "windows_expand",
    .world = {
      INSTALL_WORLD_WINDOWS,
      .registry = { .path = "%X%;C:\\old", .expand = true },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .install = {
        { SPN_INSTALL_ACTION_CREATE_DIR, "C:/u/.spn/bin" },
        { SPN_INSTALL_ACTION_INSTALL_EXE, "C:/u/.spn/bin/spn.exe", .src = WIN_EXE },
      },
      .path = {
        { SPN_INSTALL_ACTION_SET_USER_PATH, .text = WIN_BIN ";%X%;C:\\old", .expand = true },
      },
    },
  },
  {
    .name = "windows_repair",
    .world = {
      .vars = { { "USERPROFILE", "C:\\u" }, { "PATH", "C:\\p" } },
      .os = SPN_INSTALL_OS_WINDOWS,
      .exe = "c:/U/.spn/bin/SPN.EXE",
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .path = {
        { SPN_INSTALL_ACTION_SET_USER_PATH, .text = WIN_BIN },
      },
    },
  },
  {
    .name = "repair",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" } },
      .exe = "/h/.spn/bin/spn",
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .path = {
        { SPN_INSTALL_ACTION_WRITE_FILE, "/h/.spn/env", .text = ENV_SH },
        { SPN_INSTALL_ACTION_WRITE_FILE, "/h/.config/fish/conf.d/spn.fish", .text = FISH_SH },
        { SPN_INSTALL_ACTION_APPEND_LINE, "/h/.profile", .text = RC_APPEND },
        { SPN_INSTALL_ACTION_APPEND_LINE, "/h/.zshrc", .text = RC_APPEND },
      },
    },
  },
  {
    .name = "noop",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p:/h/.spn/bin" } },
      .exe = "/h/.spn/bin/spn",
      .rc = {
        [INSTALL_RC_PROFILE] = { .exists = true, .has_line = true },
        [INSTALL_RC_ZSHRC] = { .exists = true, .has_line = true },
      },
    },
  },
};

sp_test_each(install_plan, actions, test_t, tests) {
  spn_install_layout_t layout = sp_zero;
  spn_install_facts_t facts = sp_zero;
  install_world_t world = it->world;
  sp_try(install_build(t, &world, &layout, &facts));

  spn_install_plan_t plan = spn_install_plan(sp_test_arena(t), &layout, &facts);

  sp_expect_eq(t, (u32)it->expect.state, (u32)plan.state);
  install_expect_action_arr(t, plan.install, plan.num_install, it->expect.install);
  install_expect_action_arr(t, plan.path, plan.num_path, it->expect.path);
  return SP_OK;
}
