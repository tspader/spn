#include "install/install.h"

#include "sp.h"
#include "sp/os.h"
#include "sp/str.h"

spn_install_os_t spn_install_os_host() {
#if defined(SP_WIN32)
  return SPN_INSTALL_OS_WINDOWS;
#else
  return SPN_INSTALL_OS_UNIX;
#endif
}

static bool root_chars_valid(sp_str_t root) {
  sp_str_for(root, it) {
    c8 c = root.data[it];
    if (c == '"' || c == '$' || c == '`' || c == '\\') {
      return false;
    }
  }
  return true;
}

// @spader sp_fs_normalize_path?
static sp_str_t get_home_path(sp_mem_t mem, spn_install_os_t os, sp_env_t* env) {
  switch (os) {
    case SPN_INSTALL_OS_UNIX: {
      sp_str_t home = sp_env_get(env, sp_str_lit("HOME"));
      return sp_str_empty(home) ? home : sp_fs_normalize_path(mem, home);
    }
    case SPN_INSTALL_OS_WINDOWS: {
      // @spader simpler?
      struct { sp_str_t profile; sp_str_t drive; sp_str_t path; } e = {
        sp_env_get_c(env, "USERPROFILE"),
        sp_env_get_c(env, "HOMEDRIVE"),
        sp_env_get_c(env, "HOMEPATH"),
      };
      if (!sp_str_empty(e.profile)) {
        return sp_fs_normalize_path(mem, e.profile);
      }
      if (sp_str_empty(e.drive) || sp_str_empty(e.path)) {
        return sp_zero_s(sp_str_t);
      }
      return sp_str_concat(mem, e.drive, sp_fs_normalize_path(mem, e.path));
    }
  }
  return sp_zero_s(sp_str_t);
}

static sp_str_t to_native(sp_mem_t mem, spn_install_os_t os, sp_str_t path) {
  switch (os) {
    case SPN_INSTALL_OS_UNIX: return path;
    case SPN_INSTALL_OS_WINDOWS: return sp_str_replace_c8(mem, path, '/', '\\');
  }
  return path;
}

bool spn_install_path_equal(spn_install_os_t os, sp_str_t a, sp_str_t b) {
  switch (os) {
    case SPN_INSTALL_OS_UNIX: return sp_str_equal(a, b);
    case SPN_INSTALL_OS_WINDOWS: return sp_str_iequal(a, b);
  }
  return false;
}

static void resolve_rc(sp_mem_t mem, sp_env_t* env, sp_str_t home, spn_install_layout_t* layout) {
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".profile")), .always = true };
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".bashrc")) };
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".bash_profile")) };
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, home, sp_str_lit(".bash_login")) };

  sp_str_t zdotdir = sp_env_get(env, sp_str_lit("ZDOTDIR"));
  zdotdir = sp_str_empty(zdotdir) ? home : sp_fs_normalize_path(mem, zdotdir);
  layout->rc[layout->num_rc++] = (spn_install_rc_t) { .path = sp_fs_join_path(mem, zdotdir, sp_str_lit(".zshrc")), .always = true };

  sp_str_t config = sp_env_get(env, sp_str_lit("XDG_CONFIG_HOME"));
  config = sp_str_empty(config) ? sp_fs_join_path(mem, home, sp_str_lit(".config")) : sp_fs_normalize_path(mem, config);
  layout->fish_conf = sp_fs_join_path(mem, config, sp_str_lit("fish/conf.d/spn.fish"));
}

static void resolve_path(sp_mem_t mem, spn_install_os_t os, sp_env_t* env, spn_install_layout_t* layout) {
  c8 sep = os == SPN_INSTALL_OS_WINDOWS ? ';' : ':';
  sp_str_t exe_name = os == SPN_INSTALL_OS_WINDOWS ? sp_str_lit("spn.exe") : sp_str_lit("spn");

  layout->shadows = sp_da_new(mem, sp_str_t);
  sp_da(sp_str_t) entries = sp_str_split_c8(mem, sp_env_get_path(env), sep);
  sp_da_for(entries, it) {
    if (sp_str_empty(entries[it])) {
      continue;
    }
    sp_str_t entry = sp_fs_normalize_path(mem, entries[it]);
    if (spn_install_path_equal(os, entry, layout->bin)) {
      layout->on_path = true;
    }
    else if (!layout->on_path) {
      sp_da_push(layout->shadows, sp_fs_join_path(mem, entry, exe_name));
    }
  }
}

spn_install_layout_t spn_install_resolve(sp_mem_t mem, spn_install_os_t os, sp_env_t* env) {
  spn_install_layout_t layout = { .os = os };

  sp_str_t install = sp_env_get(env, sp_str_lit("SPN_INSTALL"));
  sp_str_t home = get_home_path(mem, os, env);

  if (!sp_str_empty(install)) {
    if (os == SPN_INSTALL_OS_UNIX && !root_chars_valid(install)) {
      layout.err = SPN_INSTALL_ERR_ROOT_CHARS;
      return layout;
    }
    layout.root = sp_fs_normalize_path(mem, install);
    layout.root_expr = layout.root;
  }
  else {
    if (sp_str_empty(home)) {
      layout.err = SPN_INSTALL_ERR_NO_HOME;
      return layout;
    }
    layout.root = sp_fs_join_path(mem, home, sp_str_lit(".spn"));
    layout.root_expr = sp_str_lit("$HOME/.spn");
  }

  layout.bin = sp_fs_join_path(mem, layout.root, sp_str_lit("bin"));
  layout.bin_native = to_native(mem, os, layout.bin);
  layout.exe = sp_fs_join_path(mem, layout.bin, os == SPN_INSTALL_OS_WINDOWS ? sp_str_lit("spn.exe") : sp_str_lit("spn"));

  if (os == SPN_INSTALL_OS_UNIX) {
    layout.env_file = sp_fs_join_path(mem, layout.root, sp_str_lit("env"));
    layout.rc_line = sp_fmt(mem, ". \"{}/env\"", sp_fmt_str(layout.root_expr)).value;
    if (!sp_str_empty(home)) {
      resolve_rc(mem, env, home, &layout);
    }
  }
  else {
    layout.root_expr = sp_zero_s(sp_str_t);
  }

  resolve_path(mem, os, env, &layout);

  sp_str_t github = sp_env_get(env, sp_str_lit("GITHUB_PATH"));
  layout.github_path = sp_str_empty(github) ? github : sp_fs_normalize_path(mem, github);
  layout.no_modify_path = !sp_str_empty(sp_env_get(env, sp_str_lit("SPN_INSTALL_NO_MODIFY_PATH")));

  return layout;
}
