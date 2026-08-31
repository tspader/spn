#include "spn_test.h"

#include "hash/digest/digest.h"
#include "render.h"

#define SHIM_VERSION "0.0.0"
#define SHIM_LINUX_ASSET "spn-x86_64-linux.tar.gz"
#define SHIM_MACOS_ASSET "spn-aarch64-macos.tar.gz"

typedef enum {
  SHIM_ERR_NONE = 0,
  SHIM_ERR_ARGS,
  SHIM_ERR_UNSUPPORTED_OS,
  SHIM_ERR_UNSUPPORTED_ARCH,
  SHIM_ERR_NO_BUILD,
  SHIM_ERR_MISMATCH,
  SHIM_ERR_DOWNLOAD,
  SHIM_ERR_NO_DOWNLOADER,
  SHIM_ERR_NO_SHA,
  SHIM_ERR_NO_TAR,
} err_t;

typedef struct {
  const c8* name;
  const c8* uname_s;
  const c8* uname_m;
  const c8* sysctl;
  bool windows_env;
  bool pass_arg;
  bool bad_url;
  bool missing_url;
  const c8* remove [3];
  bool stub_wget;
  bool stub_shasum;
  s32 spn_exit;
  struct {
    s32 rc;
    err_t err;
    const c8* target;
    const c8* missing;
    bool invoked;
    bool trampoline;
  } expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "linux",
    .expect = { .target = "x86_64-linux", .invoked = true },
  },
  {
    .name = "spn_exit",
    .spn_exit = 3,
    .expect = { .rc = 3, .target = "x86_64-linux", .invoked = true },
  },
  {
    .name = "mismatch",
    .bad_url = true,
    .expect = { .rc = 1, .err = SHIM_ERR_MISMATCH, .target = "x86_64-linux" },
  },
  {
    .name = "download_fail",
    .missing_url = true,
    .expect = { .rc = 1, .err = SHIM_ERR_DOWNLOAD, .target = "x86_64-linux" },
  },
  {
    .name = "unsupported_os",
    .uname_s = "SunOS",
    .expect = { .rc = 1, .err = SHIM_ERR_UNSUPPORTED_OS },
  },
  {
    .name = "unsupported_arch",
    .uname_m = "riscv64",
    .expect = { .rc = 1, .err = SHIM_ERR_UNSUPPORTED_ARCH },
  },
  {
    .name = "no_build",
    .uname_m = "aarch64",
    .expect = { .rc = 1, .err = SHIM_ERR_NO_BUILD, .missing = "aarch64-linux" },
  },
  {
    .name = "intel_mac",
    .uname_s = "Darwin",
    .sysctl = "0",
    .expect = { .rc = 1, .err = SHIM_ERR_NO_BUILD, .missing = "x86_64-macos" },
  },
  {
    .name = "rosetta",
    .uname_s = "Darwin",
    .sysctl = "1",
    .expect = { .target = "aarch64-macos", .invoked = true },
  },
  {
    .name = "trampoline",
    .windows_env = true,
    .expect = { .trampoline = true },
  },
  {
    .name = "args",
    .pass_arg = true,
    .expect = { .rc = 1, .err = SHIM_ERR_ARGS },
  },
  {
    .name = "no_downloader",
    .remove = { "curl", "wget" },
    .expect = { .rc = 1, .err = SHIM_ERR_NO_DOWNLOADER },
  },
  {
    .name = "wget_fallback",
    .remove = { "curl" },
    .stub_wget = true,
    .expect = { .target = "x86_64-linux", .invoked = true },
  },
  {
    .name = "shasum_fallback",
    .remove = { "sha256sum" },
    .stub_shasum = true,
    .expect = { .target = "x86_64-linux", .invoked = true },
  },
  {
    .name = "no_sha",
    .remove = { "sha256sum", "shasum" },
    .expect = { .rc = 1, .err = SHIM_ERR_NO_SHA },
  },
  {
    .name = "no_tar",
    .remove = { "tar" },
    .expect = { .rc = 1, .err = SHIM_ERR_NO_TAR },
  },
};

typedef struct {
  sp_mem_t mem;
  sp_str_t dir;
  sp_str_t bin;
  sp_str_t fix;
  sp_str_t home;
  sp_str_t tmp;
  sp_str_t url;
  sp_str_t sh;
  sp_str_t good_sha;
  sp_str_t bad_sha;
} fixture_t;

static sp_str_t which(sp_mem_t mem, const c8* tool) {
  sp_str_t path = sp_os_env_get(sp_str_lit("PATH"));
  sp_da(sp_str_t) dirs = sp_str_split_c8(mem, path, ':');
  sp_da_for(dirs, at) {
    if (sp_str_empty(dirs[at])) {
      continue;
    }
    sp_str_t candidate = sp_fs_join_path(mem, dirs[at], sp_cstr_as_str(tool));
    if (sp_fs_is_target_file(candidate)) {
      return candidate;
    }
  }
  return sp_zero_s(sp_str_t);
}

static sp_err_t stub_at(sp_test_t* t, fixture_t* fx, sp_str_t path, sp_str_t body) {
  sp_fs_remove_file(path);
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_file_str(path, body));
  sp_ps_output_t chmod = sp_ps_run(fx->mem, (sp_ps_config_t) {
    .command = sp_str_lit("chmod"),
    .args = { sp_str_lit("+x"), path },
  });
  sp_must_eq(t, 0, chmod.status.exit_code);
  return SP_OK;
}

static sp_err_t stub(sp_test_t* t, fixture_t* fx, const c8* name, sp_str_t body) {
  return stub_at(t, fx, sp_fs_join_path(fx->mem, fx->bin, sp_cstr_as_str(name)), body);
}

static sp_err_t build_fixture(sp_test_t* t, const test_t* it, fixture_t* fx) {
  sp_mem_t mem = fx->mem;

  fx->bin = sp_fs_join_path(mem, fx->dir, sp_str_lit("bin"));
  fx->fix = sp_fs_join_path(mem, fx->dir, sp_str_lit("fix"));
  fx->home = sp_fs_join_path(mem, fx->dir, sp_str_lit("home"));
  fx->tmp = sp_fs_join_path(mem, fx->dir, sp_str_lit("tmp"));
  sp_str_t tree = sp_fs_join_path(mem, fx->dir, sp_str_lit("tree"));
  sp_str_t bad = sp_fs_join_path(mem, fx->dir, sp_str_lit("bad"));
  sp_str_t empty = sp_fs_join_path(mem, fx->dir, sp_str_lit("empty"));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(fx->bin));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(fx->fix));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(fx->home));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(fx->tmp));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(tree));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(bad));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_dir(empty));

  fx->sh = which(mem, "sh");
  sp_must(t, !sp_str_empty(fx->sh));

  static const c8* tools [] = { "mktemp", "rm", "cut", "tar", "gzip", "curl", "wget", "sha256sum", "shasum" };
  sp_carr_for(tools, at) {
    sp_str_t real = which(mem, tools[at]);
    if (!sp_str_empty(real)) {
      sp_fs_create_sym_link(real, sp_fs_join_path(mem, fx->bin, sp_cstr_as_str(tools[at])));
    }
  }

  sp_try(stub_at(t, fx, sp_fs_join_path(mem, tree, sp_str_lit("spn")), sp_fmt(mem, "#!/bin/sh\nprintf '%s\\n' \"$@\" > args\nexit {}\n", sp_fmt_int(it->spn_exit)).value));
  sp_ps_output_t tar = sp_ps_run(mem, (sp_ps_config_t) {
    .command = sp_str_lit("tar"),
    .args = {
      sp_str_lit("-czf"), sp_fs_join_path(mem, fx->fix, sp_str_lit(SHIM_LINUX_ASSET)),
      sp_str_lit("-C"), tree,
      sp_str_lit("spn"),
    },
  });
  sp_must_eq(t, 0, tar.status.exit_code);

  sp_str_t asset = sp_fs_join_path(mem, fx->fix, sp_str_lit(SHIM_LINUX_ASSET));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_copy_file(asset, sp_fs_join_path(mem, fx->fix, sp_str_lit(SHIM_MACOS_ASSET))));

  sp_str_t corrupt = sp_fs_join_path(mem, bad, sp_str_lit(SHIM_LINUX_ASSET));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_copy_file(asset, corrupt));
  sp_str_t bytes = sp_zero;
  sp_must_eq(t, (u32)SP_OK, (u32)sp_io_read_file(mem, corrupt, &bytes));
  sp_must_eq(t, (u32)SP_OK, (u32)sp_fs_create_file_str(corrupt, sp_str_concat(mem, bytes, sp_str_lit("x"))));

  sp_must_eq(t, (u32)SPN_OK, (u32)spn_digest_file_hex(SPN_DIGEST_SHA256, mem, asset, &fx->good_sha));
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_digest_file_hex(SPN_DIGEST_SHA256, mem, corrupt, &fx->bad_sha));

  sp_str_t shasums = sp_fmt(mem, "{}  {}\n{}  {}\n",
    sp_fmt_str(fx->good_sha), sp_fmt_cstr(SHIM_LINUX_ASSET),
    sp_fmt_str(fx->good_sha), sp_fmt_cstr(SHIM_MACOS_ASSET)).value;
  installer_result_t rendered = installer_render(mem, (installer_config_t) {
    .shasums = shasums,
    .scripts = test_repo_path(mem, sp_str_lit("tools/install")),
    .out = fx->fix,
    .version = sp_str_lit(SHIM_VERSION),
    .tag = sp_str_lit("v" SHIM_VERSION),
    .repo = sp_str_lit("A/B"),
  });
  sp_must_eq(t, (u32)INSTALLER_OK, (u32)rendered.err);

  fx->url = sp_fmt(mem, "file://{}", sp_fmt_str(fx->fix)).value;
  if (it->bad_url) {
    fx->url = sp_fmt(mem, "file://{}", sp_fmt_str(bad)).value;
  }
  if (it->missing_url) {
    fx->url = sp_fmt(mem, "file://{}", sp_fmt_str(empty)).value;
  }

  const c8* uname_s = it->uname_s ? it->uname_s : "Linux";
  const c8* uname_m = it->uname_m ? it->uname_m : "x86_64";
  sp_try(stub(t, fx, "uname", sp_fmt(mem, "#!/bin/sh\ncase \"$1\" in\n  -s) echo {} ;;\n  -m) echo {} ;;\nesac\n", sp_fmt_cstr(uname_s), sp_fmt_cstr(uname_m)).value));
  if (it->sysctl) {
    sp_try(stub(t, fx, "sysctl", sp_fmt(mem, "#!/bin/sh\necho {}\n", sp_fmt_cstr(it->sysctl)).value));
  }
  if (it->windows_env) {
    sp_try(stub(t, fx, "powershell", sp_str_lit("#!/bin/sh\nprintf '%s\\n' \"$*\" > ps-args\n")));
  }
  if (it->stub_wget) {
    sp_try(stub(t, fx, "wget", sp_fmt(mem, "#!/bin/sh\nexec {} -fSsL -o \"$3\" \"$4\"\n", sp_fmt_str(which(mem, "curl"))).value));
  }
  if (it->stub_shasum) {
    sp_try(stub(t, fx, "shasum", sp_fmt(mem, "#!/bin/sh\nshift 2\nexec {} \"$@\"\n", sp_fmt_str(which(mem, "sha256sum"))).value));
  }
  sp_carr_for(it->remove, at) {
    if (it->remove[at]) {
      sp_fs_remove_file(sp_fs_join_path(mem, fx->bin, sp_cstr_as_str(it->remove[at])));
    }
  }
  return SP_OK;
}

static sp_str_t expect_err(const test_t* it, fixture_t* fx) {
  switch (it->expect.err) {
    case SHIM_ERR_NONE: {
      return sp_str_lit("");
    }
    case SHIM_ERR_ARGS: {
      return sp_str_lit("install: the spn installer takes no arguments; configure it with SPN_INSTALL_DOWNLOAD_URL and SPN_INSTALL_NO_MODIFY_PATH\n");
    }
    case SHIM_ERR_UNSUPPORTED_OS: {
      return sp_fmt(fx->mem, "install: unsupported operating system {}\n", sp_fmt_cstr(it->uname_s)).value;
    }
    case SHIM_ERR_UNSUPPORTED_ARCH: {
      return sp_fmt(fx->mem, "install: unsupported architecture {}\n", sp_fmt_cstr(it->uname_m)).value;
    }
    case SHIM_ERR_NO_BUILD: {
      return sp_fmt(fx->mem, "install: spn {} has no build for {}; it ships for: aarch64-macos x86_64-linux\n", sp_fmt_cstr(SHIM_VERSION), sp_fmt_cstr(it->expect.missing)).value;
    }
    case SHIM_ERR_MISMATCH: {
      return sp_fmt(fx->mem, "install: sha256 mismatch for {} (got {}, want {}); if a release is being published right now, retry in a minute\n",
        sp_fmt_cstr(SHIM_LINUX_ASSET), sp_fmt_str(fx->bad_sha), sp_fmt_str(fx->good_sha)).value;
    }
    case SHIM_ERR_DOWNLOAD: {
      return sp_fmt(fx->mem, "install: failed to download {}/{}\n", sp_fmt_str(fx->url), sp_fmt_cstr(SHIM_LINUX_ASSET)).value;
    }
    case SHIM_ERR_NO_DOWNLOADER: {
      return sp_str_lit("install: curl or wget is required to install spn\n");
    }
    case SHIM_ERR_NO_SHA: {
      return sp_str_lit("install: sha256sum or shasum is required to install spn\n");
    }
    case SHIM_ERR_NO_TAR: {
      return sp_str_lit("install: tar is required to install spn\n");
    }
  }
  return sp_str_lit("");
}

sp_test_each(install_shim, cases, test_t, tests) {
  sp_test_skip_on_win32();

  sp_must(t, !sp_str_empty(which(sp_test_arena(t), "curl")));
  if (it->stub_shasum && sp_str_empty(which(sp_test_arena(t), "sha256sum"))) {
    return sp_test_skip(t, "sha256sum is not available");
  }

  fixture_t fx = {
    .mem = sp_test_arena(t),
    .dir = sp_test_dir(t),
  };
  sp_try(build_fixture(t, it, &fx));

  sp_ps_config_t config = {
    .command = fx.sh,
    .cwd = fx.dir,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_CREATE,
    },
    .env = {
      .mode = SP_PS_ENV_CLEAN,
      .extra = {
        { sp_str_lit("PATH"), fx.bin },
        { sp_str_lit("HOME"), fx.home },
        { sp_str_lit("TMPDIR"), fx.tmp },
        { sp_str_lit("SPN_INSTALL_DOWNLOAD_URL"), fx.url },
      },
    },
  };
  if (it->windows_env) {
    u32 slot = 0;
    while (slot < sp_carr_len(config.env.extra) && !sp_str_empty(config.env.extra[slot].key)) {
      slot++;
    }
    config.env.extra[slot] = (sp_env_var_t) { sp_str_lit("OS"), sp_str_lit("Windows_NT") };
  }
  sp_ps_config_add_arg(fx.mem, &config, sp_fs_join_path(fx.mem, fx.fix, sp_str_lit("install.sh")));
  if (it->pass_arg) {
    sp_ps_config_add_arg(fx.mem, &config, sp_str_lit("A"));
  }

  sp_ps_output_t output = sp_ps_run(fx.mem, config);
  sp_test_kv(t, "stdout", output.out);
  sp_test_kv(t, "stderr", output.err);

  sp_must_eq(t, it->expect.rc, output.status.exit_code);

  sp_str_t out = sp_zero;
  if (it->expect.target) {
    out = sp_fmt(fx.mem, "install: downloading spn {} ({})\n", sp_fmt_cstr(SHIM_VERSION), sp_fmt_cstr(it->expect.target)).value;
  }
  sp_expect_str_eq(t, output.out, out);

  sp_str_t err = expect_err(it, &fx);
  if (it->expect.err == SHIM_ERR_DOWNLOAD) {
    sp_str_t trimmed = sp_str_trim_right(output.err);
    s32 last = sp_str_find_c8_reverse(trimmed, '\n');
    sp_str_t line = last == SP_STR_NO_MATCH ? trimmed : sp_str_sub(trimmed, last + 1, (s32)trimmed.len - last - 1);
    sp_expect_str_eq(t, line, sp_str_trim_right(err));
  }
  else {
    sp_expect_str_eq(t, output.err, err);
  }

  sp_str_t args = sp_fs_join_path(fx.mem, fx.dir, sp_str_lit("args"));
  if (it->expect.invoked) {
    sp_str_t content = sp_zero;
    sp_must_eq(t, (u32)SP_OK, (u32)sp_io_read_file(fx.mem, args, &content));
    sp_expect_str_eq_c(t, content, "self\ninstall\n--auto\n");
  }
  else {
    sp_expect(t, !sp_fs_exists(args));
  }

  sp_str_t ps_args = sp_fs_join_path(fx.mem, fx.dir, sp_str_lit("ps-args"));
  if (it->expect.trampoline) {
    sp_str_t content = sp_zero;
    sp_must_eq(t, (u32)SP_OK, (u32)sp_io_read_file(fx.mem, ps_args, &content));
    sp_expect_str_eq(t, content, sp_fmt(fx.mem, "-NoProfile -Command irm '{}/install.ps1' | iex\n", sp_fmt_str(fx.url)).value);
  }
  else {
    sp_expect(t, !sp_fs_exists(ps_args));
  }

  sp_da(sp_fs_entry_t) staged = sp_zero;
  sp_fs_collect(fx.mem, fx.tmp, &staged);
  sp_expect_eq(t, 0, sp_da_size(staged));

  sp_da(sp_fs_entry_t) home = sp_zero;
  sp_fs_collect(fx.mem, fx.home, &home);
  sp_expect_eq(t, 0, sp_da_size(home));
  return SP_OK;
}
