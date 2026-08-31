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

#define RC_LINE ". \"$HOME/.spn/env\""
#define RC_APPEND "\n" RC_LINE "\n"

#define WIN_EXE "C:/s/spn.exe"
#define WIN_BIN "C:\\u\\.spn\\bin"

#define UNIX_SHELLS \
  .fish = true, \
  .rc = { \
    [INSTALL_RC_PROFILE] = { .exists = true }, \
    [INSTALL_RC_ZSHRC] = { .exists = true }, \
  }

#define EXE_UNIX { SPN_INSTALL_ACTION_INSTALL_EXE, SPN_INSTALL_ROLE_EXE, "/h/.spn/bin/spn", .src = "/s/spn" }
#define EXE_WINDOWS { SPN_INSTALL_ACTION_INSTALL_EXE, SPN_INSTALL_ROLE_EXE, "C:/u/.spn/bin/spn.exe", .src = WIN_EXE }
#define ENV_UNIX { SPN_INSTALL_ACTION_WRITE_FILE, SPN_INSTALL_ROLE_ENV, "/h/.spn/env", .text = ENV_SH }
#define FISH_UNIX { SPN_INSTALL_ACTION_WRITE_FILE, SPN_INSTALL_ROLE_HOOK, "/h/.config/fish/conf.d/spn.fish", .text = FISH_SH }
#define RC_UNIX(path) { SPN_INSTALL_ACTION_APPEND_LINE, SPN_INSTALL_ROLE_HOOK, path, .text = RC_APPEND, .line = RC_LINE }

typedef struct {
  const c8* name;
  install_world_t world;
  struct {
    bool set;
    spn_install_choices_t value;
  } choices;
  struct {
    spn_install_path_state_t state;
    u32 live;
    bool posix;
    install_action_spec_t actions [SPN_INSTALL_MAX_ACTIONS + 1];
  } expect;
} test_t;

static const test_t tests [] = {
  {
    // nothing in $HOME says which shell this is, so .profile carries it
    .name = "fresh",
    .world = { INSTALL_WORLD_UNIX },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        RC_UNIX("/h/.profile"),
      },
    },
  },
  {
    .name = "fresh_every_shell",
    .world = { INSTALL_WORLD_UNIX, UNIX_SHELLS },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        FISH_UNIX,
        RC_UNIX("/h/.profile"),
        RC_UNIX("/h/.zshenv"),
      },
    },
  },
  {
    // a fresh macos account: no config files at all, but $SHELL says zsh
    .name = "fresh_macos",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "SHELL", "/bin/zsh" } },
      .exe = "/s/spn",
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        RC_UNIX("/h/.zshenv"),
      },
    },
  },
  {
    .name = "login_fish",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" }, { "SHELL", "/usr/bin/fish" } },
      .exe = "/s/spn",
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        FISH_UNIX,
      },
    },
  },
  {
    .name = "default_zsh_only",
    .world = {
      INSTALL_WORLD_UNIX,
      .rc = { [INSTALL_RC_ZSHRC] = { .exists = true } },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        RC_UNIX("/h/.zshenv"),
      },
    },
  },
  {
    .name = "default_fish_only",
    .world = { INSTALL_WORLD_UNIX, .fish = true },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        FISH_UNIX,
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
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        RC_UNIX("/h/.profile"),
        RC_UNIX("/h/.bashrc"),
      },
    },
  },
  {
    .name = "has_line",
    .world = {
      INSTALL_WORLD_UNIX,
      .fish = true,
      .rc = {
        [INSTALL_RC_PROFILE] = { .exists = true, .has_line = true },
        [INSTALL_RC_ZSHRC] = { .exists = true },
      },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .live = 1,
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        FISH_UNIX,
        RC_UNIX("/h/.zshenv"),
      },
    },
  },
  {
    // an older spn hooked .zshrc; it still carries us, and .zshenv takes over
    .name = "zshrc_has_line",
    .world = {
      INSTALL_WORLD_UNIX,
      .rc = { [INSTALL_RC_ZSHRC] = { .exists = true, .has_line = true } },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .live = 1,
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        RC_UNIX("/h/.zshenv"),
      },
    },
  },
  {
    // everything already carries us; nothing to write, but PATH is still
    // waiting on a restart
    .name = "pending",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" } },
      .exe = "/h/.spn/bin/spn",
      .env_current = true,
      .rc = {
        [INSTALL_RC_PROFILE] = { .exists = true, .has_line = true },
        [INSTALL_RC_ZSHENV] = { .exists = true, .has_line = true },
      },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .live = 2,
      .posix = true,
    },
  },
  {
    .name = "fish_current",
    .world = {
      INSTALL_WORLD_UNIX,
      .fish = true,
      .fish_current = true,
      .env_current = true,
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .live = 1,
      .actions = { EXE_UNIX },
    },
  },
  {
    .name = "on_path",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p:/h/.spn/bin" } },
      .exe = "/s/spn",
    },
    .expect = {
      .actions = { EXE_UNIX },
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
      .actions = { EXE_UNIX },
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
      .actions = { EXE_UNIX },
    },
  },
  {
    .name = "on_path_beats_ci",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p:/h/.spn/bin" }, { "GITHUB_PATH", "/gh" } },
      .exe = "/s/spn",
    },
    .expect = {
      .actions = { EXE_UNIX },
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
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        { SPN_INSTALL_ACTION_APPEND_LINE, SPN_INSTALL_ROLE_HOOK, "/gh", .text = "/h/.spn/bin\n", .line = "/h/.spn/bin" },
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
      .actions = {
        EXE_WINDOWS,
        { SPN_INSTALL_ACTION_APPEND_LINE, SPN_INSTALL_ROLE_HOOK, "C:/gh", .text = WIN_BIN "\n", .line = WIN_BIN },
      },
    },
  },
  {
    .name = "windows_fresh",
    .world = { INSTALL_WORLD_WINDOWS },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .actions = {
        EXE_WINDOWS,
        { SPN_INSTALL_ACTION_SET_USER_PATH, SPN_INSTALL_ROLE_HOOK, .text = WIN_BIN, .reg = SPN_INSTALL_REG_SZ },
      },
    },
  },
  {
    .name = "windows_prepend",
    .world = {
      INSTALL_WORLD_WINDOWS,
      .registry = { .path = "C:\\old", .kind = SPN_INSTALL_REG_SZ },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .actions = {
        EXE_WINDOWS,
        { SPN_INSTALL_ACTION_SET_USER_PATH, SPN_INSTALL_ROLE_HOOK, .text = WIN_BIN ";C:\\old", .reg = SPN_INSTALL_REG_SZ },
      },
    },
  },
  {
    .name = "windows_registry_present",
    .world = {
      INSTALL_WORLD_WINDOWS,
      .registry = { .path = "c:\\U\\.spn\\BIN;C:\\old", .kind = SPN_INSTALL_REG_SZ },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .live = 1,
      .actions = { EXE_WINDOWS },
    },
  },
  {
    .name = "windows_expand",
    .world = {
      INSTALL_WORLD_WINDOWS,
      .registry = { .path = "%X%;C:\\old", .kind = SPN_INSTALL_REG_EXPAND },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .actions = {
        EXE_WINDOWS,
        { SPN_INSTALL_ACTION_SET_USER_PATH, SPN_INSTALL_ROLE_HOOK, .text = WIN_BIN ";%X%;C:\\old", .reg = SPN_INSTALL_REG_EXPAND },
      },
    },
  },
  {
    .name = "windows_registry_other",
    .world = {
      INSTALL_WORLD_WINDOWS,
      .registry = { .kind = SPN_INSTALL_REG_OTHER },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_MANUAL,
      .actions = { EXE_WINDOWS },
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
      .actions = {
        { SPN_INSTALL_ACTION_SET_USER_PATH, SPN_INSTALL_ROLE_HOOK, .text = WIN_BIN, .reg = SPN_INSTALL_REG_SZ },
      },
    },
  },
  {
    .name = "repair",
    .world = {
      .vars = { { "HOME", "/h" }, { "PATH", "/p" } },
      .exe = "/h/.spn/bin/spn",
      UNIX_SHELLS,
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .posix = true,
      .actions = {
        ENV_UNIX,
        FISH_UNIX,
        RC_UNIX("/h/.profile"),
        RC_UNIX("/h/.zshenv"),
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
        [INSTALL_RC_ZSHENV] = { .exists = true, .has_line = true },
      },
    },
  },
  {
    .name = "opt_out",
    .world = { INSTALL_WORLD_UNIX },
    .choices = { .set = true },
    .expect = {
      .state = SPN_INSTALL_PATH_MANUAL,
      .actions = { EXE_UNIX },
    },
  },
  {
    .name = "fish_only",
    .world = { INSTALL_WORLD_UNIX },
    .choices = { .set = true, .value = { .path = { { .kind = SPN_INSTALL_SHELL_FISH } }, .num_path = 1 } },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        FISH_UNIX,
      },
    },
  },
  {
    .name = "bash_only",
    .world = {
      INSTALL_WORLD_UNIX,
      .rc = {
        [INSTALL_RC_BASHRC] = { .exists = true },
      },
    },
    .choices = { .set = true, .value = { .path = { { .kind = SPN_INSTALL_SHELL_BASH } }, .num_path = 1 } },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        RC_UNIX("/h/.profile"),
        RC_UNIX("/h/.bashrc"),
      },
    },
  },
  {
    .name = "zsh_only",
    .world = { INSTALL_WORLD_UNIX },
    .choices = { .set = true, .value = { .path = { { .kind = SPN_INSTALL_SHELL_ZSH } }, .num_path = 1 } },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        RC_UNIX("/h/.zshenv"),
      },
    },
  },
  {
    .name = "custom_file",
    .world = { INSTALL_WORLD_UNIX },
    .choices = { .set = true, .value = { .path = { { .kind = SPN_INSTALL_SHELL_CUSTOM, .custom = { .data = "/c/rc", .len = 5 } } }, .num_path = 1 } },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        RC_UNIX("/c/rc"),
      },
    },
  },
  {
    .name = "custom_file_has_line",
    .world = { INSTALL_WORLD_UNIX },
    .choices = { .set = true, .value = { .path = { { .kind = SPN_INSTALL_SHELL_CUSTOM, .custom = { .data = "/c/rc", .len = 5 }, .has_line = true } }, .num_path = 1 } },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .live = 1,
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
      },
    },
  },
  {
    .name = "custom_file_is_rc",
    .world = { INSTALL_WORLD_UNIX },
    .choices = {
      .set = true,
      .value = {
        .path = {
          { .kind = SPN_INSTALL_SHELL_ZSH },
          { .kind = SPN_INSTALL_SHELL_CUSTOM, .custom = { .data = "/h/.zshenv", .len = 10 } },
        },
        .num_path = 2,
      },
    },
    .expect = {
      .state = SPN_INSTALL_PATH_UPDATED,
      .posix = true,
      .actions = {
        EXE_UNIX,
        ENV_UNIX,
        RC_UNIX("/h/.zshenv"),
      },
    },
  },
  {
    .name = "windows_opt_out",
    .world = { INSTALL_WORLD_WINDOWS },
    .choices = { .set = true },
    .expect = {
      .state = SPN_INSTALL_PATH_MANUAL,
      .actions = { EXE_WINDOWS },
    },
  },
};

sp_test_each(install_plan, actions, test_t, tests) {
  spn_install_layout_t layout = sp_zero;
  spn_install_facts_t facts = sp_zero;
  install_world_t world = it->world;
  sp_try(install_build(t, &world, &layout, &facts));

  spn_install_choices_t choices = it->choices.set ? it->choices.value : spn_install_choices(&layout, &facts);
  spn_install_plan_t plan = spn_install_plan(sp_test_arena(t), &layout, &facts, &choices);

  sp_expect_eq(t, (u32)it->expect.state, (u32)plan.state);
  sp_expect_eq(t, it->expect.live, plan.live);
  sp_expect_eq(t, it->expect.posix, plan.posix);
  install_expect_action_arr(t, plan.actions, plan.count, it->expect.actions);
  return SP_OK;
}
