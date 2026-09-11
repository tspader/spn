#include "toolchain/search.h"

#include "paths/paths.h"
#include "str/str.h"

spn_search_rules_t spn_search_rules(spn_os_t os) {
  switch (os) {
    case SPN_OS_WINDOWS: {
      return (spn_search_rules_t) {
        .sep = ';',
        .kind = SP_FS_PATH_WINDOWS,
        .suffix = { "", ".exe" },
      };
    }
    case SPN_OS_LINUX:
    case SPN_OS_MACOS: {
      return (spn_search_rules_t) {
        .sep = ':',
        .kind = SP_FS_PATH_POSIX,
        .suffix = { "" },
      };
    }
    case SPN_OS_WASI:
    case SPN_OS_FREESTANDING:
    case SPN_OS_NONE: {
      sp_unreachable_case();
    }
  }
  sp_unreachable_return(sp_zero_s(spn_search_rules_t));
}

sp_da(sp_str_t) spn_search_dirs(spn_search_rules_t rules, sp_mem_t mem, sp_str_t path) {
  sp_da(sp_str_t) dirs = sp_da_new(mem, sp_str_t);
  sp_str_for_word(path, rules.sep, it) {
    sp_str_t dir = sp_fs_normalize_path(mem, sp_fs_trim_path(it.entry));
    if (sp_fs_is_absolute_for(dir, rules.kind) && spn_path_normal(dir)) {
      sp_da_push(dirs, dir);
    }
  }
  return dirs;
}

static sp_str_t existing(spn_search_rules_t rules, sp_mem_t mem, sp_str_t stem) {
  sp_carr_for_until(rules.suffix, it, rules.suffix[it]) {
    sp_str_t candidate = sp_str_concat(mem, stem, sp_cstr_as_str(rules.suffix[it]));
    if (sp_fs_is_target_file(candidate)) {
      return candidate;
    }
  }
  return sp_str_lit("");
}

sp_str_t spn_search_program(spn_search_rules_t rules, sp_mem_t mem, const spn_path_roots_t* roots, spn_arg_t program, sp_da(sp_str_t) dirs) {
  if (!spn_path_empty(program.path)) {
    return existing(rules, mem, spn_path_str(roots, mem, program.path));
  }
  sp_da_for(dirs, it) {
    sp_str_t found = existing(rules, mem, sp_fs_join_path(mem, dirs[it], program.prefix));
    if (!sp_str_empty(found)) {
      return found;
    }
  }
  return sp_str_lit("");
}

sp_str_t spn_search_prepend(spn_search_rules_t rules, sp_mem_t mem, sp_str_t dir, sp_str_t path) {
  return sp_fmt(mem, "{}{}{}", sp_fmt_str(dir), sp_fmt_char(rules.sep), sp_fmt_str(path)).value;
}
