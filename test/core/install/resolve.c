#include "harness.h"

typedef struct {
  const c8* name;
  install_var_t vars [INSTALL_MAX_VARS];
  spn_install_os_t os;
  struct {
    spn_install_err_t err;
    const c8* root;
    const c8* root_expr;
    const c8* bin;
    const c8* bin_native;
    const c8* exe;
    const c8* env_file;
    const c8* rc_line;
  } expect;
} root_test_t;

static const root_test_t root_tests [] = {
  {
    .name = "home",
    .vars = { { "HOME", "/h" } },
    .expect = {
      .root = "/h/.spn",
      .root_expr = "$HOME/.spn",
      .bin = "/h/.spn/bin",
      .bin_native = "/h/.spn/bin",
      .exe = "/h/.spn/bin/spn",
      .env_file = "/h/.spn/env",
      .rc_line = ". \"$HOME/.spn/env\"",
    },
  },
  {
    .name = "custom",
    .vars = { { "HOME", "/h" }, { "SPN_INSTALL", "/opt/spn" } },
    .expect = {
      .root = "/opt/spn",
      .root_expr = "/opt/spn",
      .bin = "/opt/spn/bin",
      .bin_native = "/opt/spn/bin",
      .exe = "/opt/spn/bin/spn",
      .env_file = "/opt/spn/env",
      .rc_line = ". \"/opt/spn/env\"",
    },
  },
  {
    .name = "custom_no_home",
    .vars = { { "SPN_INSTALL", "/opt/spn" } },
    .expect = {
      .root = "/opt/spn",
      .root_expr = "/opt/spn",
      .bin = "/opt/spn/bin",
      .bin_native = "/opt/spn/bin",
      .exe = "/opt/spn/bin/spn",
      .env_file = "/opt/spn/env",
      .rc_line = ". \"/opt/spn/env\"",
    },
  },
  {
    .name = "no_home",
    .vars = { { "PATH", "/p" } },
    .expect = { .err = SPN_INSTALL_ERR_NO_HOME },
  },
  {
    .name = "quote",
    .vars = { { "HOME", "/h" }, { "SPN_INSTALL", "/o\"k" } },
    .expect = { .err = SPN_INSTALL_ERR_ROOT_CHARS },
  },
  {
    .name = "dollar",
    .vars = { { "HOME", "/h" }, { "SPN_INSTALL", "/o$k" } },
    .expect = { .err = SPN_INSTALL_ERR_ROOT_CHARS },
  },
  {
    .name = "backtick",
    .vars = { { "HOME", "/h" }, { "SPN_INSTALL", "/o`k" } },
    .expect = { .err = SPN_INSTALL_ERR_ROOT_CHARS },
  },
  {
    .name = "backslash",
    .vars = { { "HOME", "/h" }, { "SPN_INSTALL", "/o\\k" } },
    .expect = { .err = SPN_INSTALL_ERR_ROOT_CHARS },
  },
  {
    .name = "install_empty",
    .vars = { { "HOME", "/h" }, { "SPN_INSTALL", "" } },
    .expect = {
      .root = "/h/.spn",
      .root_expr = "$HOME/.spn",
      .bin = "/h/.spn/bin",
      .bin_native = "/h/.spn/bin",
      .exe = "/h/.spn/bin/spn",
      .env_file = "/h/.spn/env",
      .rc_line = ". \"$HOME/.spn/env\"",
    },
  },
  {
    .name = "home_empty",
    .vars = { { "HOME", "" }, { "PATH", "/p" } },
    .expect = { .err = SPN_INSTALL_ERR_NO_HOME },
  },
  {
    .name = "windows_userprofile",
    .vars = { { "USERPROFILE", "C:\\Users\\u" } },
    .os = SPN_INSTALL_OS_WINDOWS,
    .expect = {
      .root = "C:/Users/u/.spn",
      .bin = "C:/Users/u/.spn/bin",
      .bin_native = "C:\\Users\\u\\.spn\\bin",
      .exe = "C:/Users/u/.spn/bin/spn.exe",
    },
  },
  {
    .name = "windows_homepath",
    .vars = { { "HOMEDRIVE", "C:" }, { "HOMEPATH", "\\Users\\u" } },
    .os = SPN_INSTALL_OS_WINDOWS,
    .expect = {
      .root = "C:/Users/u/.spn",
      .bin = "C:/Users/u/.spn/bin",
      .bin_native = "C:\\Users\\u\\.spn\\bin",
      .exe = "C:/Users/u/.spn/bin/spn.exe",
    },
  },
  {
    .name = "windows_no_home",
    .vars = { { "PATH", "C:\\p" } },
    .os = SPN_INSTALL_OS_WINDOWS,
    .expect = { .err = SPN_INSTALL_ERR_NO_HOME },
  },
  {
    .name = "windows_backslash_install",
    .vars = { { "USERPROFILE", "C:\\Users\\u" }, { "SPN_INSTALL", "C:\\opt\\spn" } },
    .os = SPN_INSTALL_OS_WINDOWS,
    .expect = {
      .root = "C:/opt/spn",
      .bin = "C:/opt/spn/bin",
      .bin_native = "C:\\opt\\spn\\bin",
      .exe = "C:/opt/spn/bin/spn.exe",
    },
  },
};

sp_test_each(install_resolve, root, root_test_t, root_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_env_t env = install_env(mem, it->vars);
  spn_install_layout_t layout = spn_install_resolve(mem, it->os, &env);

  sp_must_eq(t, (u32)it->expect.err, (u32)layout.err);
  if (it->expect.err) {
    return SP_OK;
  }

  sp_expect_str_eq_c(t, layout.root, it->expect.root);
  sp_expect_str_eq_c(t, layout.root_expr, it->expect.root_expr ? it->expect.root_expr : "");
  sp_expect_str_eq_c(t, layout.bin, it->expect.bin);
  sp_expect_str_eq_c(t, layout.bin_native, it->expect.bin_native);
  sp_expect_str_eq_c(t, layout.exe, it->expect.exe);
  sp_expect_str_eq_c(t, layout.env_file, it->expect.env_file ? it->expect.env_file : "");
  sp_expect_str_eq_c(t, layout.rc_line, it->expect.rc_line ? it->expect.rc_line : "");
  return SP_OK;
}


typedef struct {
  const c8* name;
  install_var_t vars [INSTALL_MAX_VARS];
  spn_install_os_t os;
  struct {
    struct {
      const c8* path;
      bool always;
    } rc [SPN_INSTALL_MAX_RC];
    const c8* fish_conf;
  } expect;
} rc_test_t;

static const rc_test_t rc_tests [] = {
  {
    .name = "home",
    .vars = { { "HOME", "/h" } },
    .expect = {
      .rc = {
        { "/h/.profile", .always = true },
        { "/h/.bashrc" },
        { "/h/.bash_profile" },
        { "/h/.bash_login" },
        { "/h/.zshrc", .always = true },
      },
      .fish_conf = "/h/.config/fish/conf.d/spn.fish",
    },
  },
  {
    .name = "zdotdir",
    .vars = { { "HOME", "/h" }, { "ZDOTDIR", "/z" } },
    .expect = {
      .rc = {
        { "/h/.profile", .always = true },
        { "/h/.bashrc" },
        { "/h/.bash_profile" },
        { "/h/.bash_login" },
        { "/z/.zshrc", .always = true },
      },
      .fish_conf = "/h/.config/fish/conf.d/spn.fish",
    },
  },
  {
    .name = "xdg",
    .vars = { { "HOME", "/h" }, { "XDG_CONFIG_HOME", "/x" } },
    .expect = {
      .rc = {
        { "/h/.profile", .always = true },
        { "/h/.bashrc" },
        { "/h/.bash_profile" },
        { "/h/.bash_login" },
        { "/h/.zshrc", .always = true },
      },
      .fish_conf = "/x/fish/conf.d/spn.fish",
    },
  },
  {
    .name = "custom_home",
    .vars = { { "HOME", "/h" }, { "SPN_INSTALL", "/opt/spn" } },
    .expect = {
      .rc = {
        { "/h/.profile", .always = true },
        { "/h/.bashrc" },
        { "/h/.bash_profile" },
        { "/h/.bash_login" },
        { "/h/.zshrc", .always = true },
      },
      .fish_conf = "/h/.config/fish/conf.d/spn.fish",
    },
  },
  {
    .name = "custom_no_home",
    .vars = { { "SPN_INSTALL", "/opt/spn" } },
  },
  {
    .name = "windows",
    .vars = { { "USERPROFILE", "C:\\Users\\u" } },
    .os = SPN_INSTALL_OS_WINDOWS,
  },
};

sp_test_each(install_resolve, rc, rc_test_t, rc_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_env_t env = install_env(mem, it->vars);
  spn_install_layout_t layout = spn_install_resolve(mem, it->os, &env);
  sp_must_eq(t, (u32)SPN_INSTALL_OK, (u32)layout.err);

  u32 num_expect = 0;
  sp_carr_detect_len(it->expect.rc, num_expect, it->expect.rc[num_expect].path);
  sp_must_eq(t, num_expect, layout.num_rc);
  sp_for(at, num_expect) {
    sp_expect_str_eq_c(t, layout.rc[at].path, it->expect.rc[at].path);
    sp_expect_eq(t, it->expect.rc[at].always, layout.rc[at].always);
  }
  sp_expect_str_eq_c(t, layout.fish_conf, it->expect.fish_conf ? it->expect.fish_conf : "");
  return SP_OK;
}


typedef struct {
  const c8* name;
  install_var_t vars [INSTALL_MAX_VARS];
  spn_install_os_t os;
  struct {
    bool on_path;
    const c8* shadows [4];
  } expect;
} path_test_t;

static const path_test_t path_tests [] = {
  {
    .name = "missing",
    .vars = { { "HOME", "/h" }, { "PATH", "/a:/b" } },
    .expect = { .shadows = { "/a/spn", "/b/spn" } },
  },
  {
    .name = "present",
    .vars = { { "HOME", "/h" }, { "PATH", "/a:/h/.spn/bin" } },
    .expect = {
      .on_path = true,
      .shadows = { "/a/spn" },
    },
  },
  {
    .name = "after_bin",
    .vars = { { "HOME", "/h" }, { "PATH", "/h/.spn/bin:/a" } },
    .expect = { .on_path = true },
  },
  {
    .name = "empty_entries",
    .vars = { { "HOME", "/h" }, { "PATH", ":/a:" } },
    .expect = { .shadows = { "/a/spn" } },
  },
  {
    .name = "no_path",
    .vars = { { "HOME", "/h" } },
  },
  {
    .name = "windows",
    .vars = { { "USERPROFILE", "C:\\u" }, { "PATH", "C:\\a;C:\\u\\.spn\\bin" } },
    .os = SPN_INSTALL_OS_WINDOWS,
    .expect = {
      .on_path = true,
      .shadows = { "C:/a/spn.exe" },
    },
  },
  {
    .name = "windows_case",
    .vars = { { "USERPROFILE", "C:\\u" }, { "PATH", "c:\\U\\.spn\\BIN" } },
    .os = SPN_INSTALL_OS_WINDOWS,
    .expect = { .on_path = true },
  },
};

sp_test_each(install_resolve, path, path_test_t, path_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_env_t env = install_env(mem, it->vars);
  spn_install_layout_t layout = spn_install_resolve(mem, it->os, &env);
  sp_must_eq(t, (u32)SPN_INSTALL_OK, (u32)layout.err);

  sp_expect_eq(t, it->expect.on_path, layout.on_path);
  sp_must_strs_eq(t, layout.shadows, sp_da_size(layout.shadows), it->expect.shadows);
  return SP_OK;
}


typedef struct {
  const c8* name;
  install_var_t vars [INSTALL_MAX_VARS];
  struct {
    const c8* github_path;
    bool no_modify_path;
  } expect;
} flags_test_t;

static const flags_test_t flags_tests [] = {
  {
    .name = "none",
    .vars = { { "HOME", "/h" } },
  },
  {
    .name = "github",
    .vars = { { "HOME", "/h" }, { "GITHUB_PATH", "/gh" } },
    .expect = { .github_path = "/gh" },
  },
  {
    .name = "github_empty",
    .vars = { { "HOME", "/h" }, { "GITHUB_PATH", "" } },
  },
  {
    .name = "no_modify",
    .vars = { { "HOME", "/h" }, { "SPN_INSTALL_NO_MODIFY_PATH", "1" } },
    .expect = { .no_modify_path = true },
  },
  {
    .name = "no_modify_zero",
    .vars = { { "HOME", "/h" }, { "SPN_INSTALL_NO_MODIFY_PATH", "0" } },
    .expect = { .no_modify_path = true },
  },
};

sp_test_each(install_resolve, flags, flags_test_t, flags_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_env_t env = install_env(mem, it->vars);
  spn_install_layout_t layout = spn_install_resolve(mem, SPN_INSTALL_OS_UNIX, &env);
  sp_must_eq(t, (u32)SPN_INSTALL_OK, (u32)layout.err);

  sp_expect_str_eq_c(t, layout.github_path, it->expect.github_path ? it->expect.github_path : "");
  sp_expect_eq(t, it->expect.no_modify_path, layout.no_modify_path);
  return SP_OK;
}
