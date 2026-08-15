#include "sp.h"
#include "sp/macro.h"
#include "sp/sp_glob.h"
#include "spn.h"

#include "api/api.h"
#include "api/types.h"
#include "core/core.h"
#include "core/types.h"
#include "ctx/types.h"
#include "event/types.h"
#include "paths/paths.h"
#include "pkg/types.h"
#include "session/types.h"
#include "target/types.h"
#include "unit/types.h"

#include "error/error.h"
#include "event/event.h"
#include "external/wasm/wasm.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "pkg/mutate.h"
#include "target/mutate.h"
#include "pkg/pkg.h"
#include "session/session.h"
#include "sp/io.h"
#include "target/target.h"

spn_pkg_unit_t* spn_api_unit(const void* opaque) {
  return (spn_pkg_unit_t*)opaque;
}

spn_path_t spn_api_dir_path(spn_pkg_unit_t* unit, spn_dir_t dir) {
  switch (dir) {
    case SPN_DIR_NONE:     return sp_zero_struct(spn_path_t);
    // Units have no distinct cache dir; CACHE aliases the store
    case SPN_DIR_CACHE:    return unit->paths.store;
    case SPN_DIR_STORE:    return unit->paths.store;
    case SPN_DIR_INCLUDE:  return unit->paths.include;
    case SPN_DIR_VENDOR:   return unit->paths.vendor;
    case SPN_DIR_LIB:      return unit->paths.lib;
    case SPN_DIR_SOURCE:   return unit->paths.roots.source;
    case SPN_DIR_WORK:     return unit->paths.work;
    case SPN_DIR_PROJECT:  return unit->session->paths.root;
    case SPN_DIR_MANIFEST: return unit->paths.roots.recipe;
  }

  SP_UNREACHABLE_RETURN(sp_zero_struct(spn_path_t));
}

sp_str_t spn_api_dir(spn_pkg_unit_t* unit, spn_dir_t dir) {
  return spn_path_str(&spn.roots, spn.mem, spn_api_dir_path(unit, dir));
}

bool spn_api_path_rejected(spn_pkg_unit_t* unit, const c8* fn, sp_str_t path) {
  if (spn_path_normal(path)) {
    return false;
  }
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t message = sp_fmt(scratch.mem, "{}: {} must not contain '.', '..', or empty components", SP_FMT_CSTR(fn), SP_FMT_STR(path)).value;
  if (!spn_wasm_trap_active(unit, message)) {
    spn_err_emit(unit->session->ctx, (spn_err_union_t) {
      .kind = SPN_ERR_PATH_COMPONENT,
      .fs = { .path = sp_str_copy(spn.mem, path) },
    });
  }
  sp_mem_end_scratch(scratch);
  return true;
}

spn_path_t spn_api_tree_path(spn_pkg_unit_t* unit, const c8* fn, const c8* path) {
  sp_str_t str = sp_str_view(path);
  if (spn_api_path_rejected(unit, fn, str)) {
    return sp_zero_struct(spn_path_t);
  }
  return spn_tree_path(spn.mem, &spn.roots, unit->paths.roots, SPN_TREE_SOURCE, str);
}

static spn_path_t api_path(spn_pkg_unit_t* unit, const c8* fn, const c8* path) {
  spn_path_t made = spn_api_tree_path(unit, fn, path);
  if (spn_path_empty(made)) {
    return made;
  }
  return spn_path_canonicalize(spn.mem, &spn.roots, made);
}

static spn_target_t* wrap(spn_pkg_unit_t* unit, spn_target_info_t* info) {
  spn_target_t* target = sp_alloc_type(spn.mem, spn_target_t);
  target->unit = unit;
  target->info = info;
  return target;
}

spn_target_t* spn_get_target(spn_t* spn, const c8* name) {
  spn_pkg_unit_t* unit = spn_api_unit(spn);
  spn_target_info_t* info = spn_pkg_get_target(unit->info, name);
  return info ? wrap(unit, info) : SP_NULLPTR;
}

spn_target_t* spn_add_exe(spn_config_t* config, const c8* name) {
  spn_pkg_unit_t* unit = spn_api_unit(config);
  return wrap(unit, spn_pkg_add_exe(unit->info, name));
}

spn_target_t* spn_add_lib(spn_config_t* config, const c8* name, spn_linkage_t kind) {
  spn_linkage_set_t linkages = sp_zero;
  spn_linkage_set_add(&linkages, kind);
  spn_pkg_unit_t* unit = spn_api_unit(config);
  return wrap(unit, spn_pkg_add_lib_ex(unit->info, spn_intern_cstr(name), linkages));
}

spn_target_t* spn_add_test(spn_config_t* config, const c8* name) {
  spn_pkg_unit_t* unit = spn_api_unit(config);
  return wrap(unit, spn_pkg_add_test(unit->info, name));
}

void spn_add_include(spn_config_t* config, const c8* path) {
  spn_pkg_unit_t* unit = spn_api_unit(config);
  spn_path_t made = api_path(unit, "spn_add_include", path);
  if (spn_path_empty(made)) {
    return;
  }
  spn_pkg_add_include(unit->info, made);
}

void spn_add_define(spn_config_t* config, const c8* define) {
  spn_pkg_add_define(spn_api_unit(config)->info, define);
}

void spn_add_system_dep(spn_config_t* config, const c8* dep) {
  spn_pkg_add_system_dep(spn_api_unit(config)->info, dep);
}

const spn_t* spn_get_dep(const spn_t* s, const c8* name) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  sp_str_t key = sp_str_view(name);

  sp_da_for(unit->info->deps, it) {
    spn_requested_dep_t* dep = &unit->info->deps[it];
    if (!sp_str_equal(spn_pkg_name_from_qualified(dep->qualified).name, key)) continue;

    spn_pkg_unit_t* dep_unit = spn_session_find_dep(unit->session, unit, dep->qualified, dep->kind);
    if (dep_unit) return (const spn_t*)dep_unit;
  }

  return SP_NULLPTR;
}

const c8* spn_get_dir(const spn_t* s, spn_dir_t dir) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  return sp_str_to_cstr(spn.mem, spn_api_dir(unit, dir));
}

const c8* spn_get_subdir(const spn_t* s, spn_dir_t base, const c8* path) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  if (spn_api_path_rejected(unit, "spn_get_subdir", sp_str_view(path))) {
    return "";
  }
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t joined = sp_fs_join_path(scratch.mem, spn_api_dir(unit, base), sp_str_view(path));
  const c8* result = sp_str_to_cstr(spn.mem, joined);
  sp_mem_end_scratch(scratch);
  return result;
}

void spn_log(spn_t* s, const c8* message) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  spn_event_buffer_push(spn.events, (spn_event_t) {
    .kind = SPN_EVENT_USER_LOG,
    .pkg = unit->info->name,
    .user_log = { .message = sp_str_from_cstr(spn.mem, message) },
  });
}

void spn_write_file(spn_t* s, const c8* path, const c8* content) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  if (spn_api_path_rejected(unit, "spn_write_file", sp_str_view(path))) {
    return;
  }
  SPN_API_LOG(unit, "spn_write_file", "{}", SP_FMT_CSTR(path));

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_path_t joined = spn_path_join(scratch.mem, unit->paths.work, sp_str_view(path));
  sp_str_t full_path = spn_path_str(&spn.roots, scratch.mem, joined);
  sp_str_t parent = sp_fs_parent_path(full_path);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }

  spn_fs_update_file_str(full_path, sp_str_view(content));
  sp_mem_end_scratch(scratch);
}

s32 spn_api_copy(sp_str_t from, sp_str_t to) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  s32 err = SPN_OK;

  // sp_fs_copy only understands a bare "*" or an exact name; expand real glob
  // patterns (e.g. "lib/*.o") ourselves against the source directory.
  sp_str_t pattern = sp_fs_get_name(from);
  if (sp_fs_is_glob(from)) {
    sp_str_t dir = sp_fs_parent_path(from);
    if (!sp_fs_is_dir(dir)) {
      err = SPN_ERROR;
    }
    else if (sp_str_equal(pattern, sp_str_lit("*"))) {
      err = sp_fs_copy(from, to);
    }
    else {
      sp_fs_create_dir(to);

      sp_glob_set_t* glob = sp_glob_set_new(scratch.mem);
      sp_glob_set_add(glob, sp_str_to_cstr(scratch.mem, pattern));
      sp_glob_set_build(glob);

      sp_da(sp_fs_entry_t) entries = sp_fs_collect(scratch.mem, dir);
      sp_da_for(entries, it) {
        if (sp_glob_set_match(glob, entries[it].name)) {
          spn_fs_update_file(sp_fs_join_path(scratch.mem, dir, entries[it].name), to);
        }
      }
    }
  }
  else if (!sp_fs_exists(from)) {
    err = SPN_ERROR;
  }
  else if (sp_fs_is_dir(from)) {
    sp_fs_create_dir(to);
    err = sp_fs_copy(from, to);
  }
  else {
    // @spader This bit me so I just patched it over like this, but
    // I need to think about how this should work
    sp_str_t parent = sp_fs_parent_path(to);
    if (!sp_str_empty(parent)) {
      sp_fs_create_dir(parent);
    }

    err = spn_fs_update_file(from, to);
  }

  sp_mem_end_scratch(scratch);
  return err;
}

s32 spn_api_copy_rooted(spn_pkg_unit_t* unit, spn_dir_t from_dir, sp_str_t from_path, spn_dir_t to_dir, sp_str_t to_path) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t from = sp_fs_join_path(scratch.mem, spn_api_dir(unit, from_dir), from_path);
  sp_str_t to = sp_fs_join_path(scratch.mem, spn_api_dir(unit, to_dir), to_path);
  SPN_API_LOG(unit, "spn_copy", "{} -> {}", SP_FMT_STR(from), SP_FMT_STR(to));

  s32 err = spn_api_copy(from, to);
  sp_mem_end_scratch(scratch);
  return err;
}

s32 spn_copy(spn_t* s, spn_dir_t from_dir, const c8* from_path, spn_dir_t to_dir, const c8* to_path) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  if (spn_api_path_rejected(unit, "spn_copy", sp_str_view(from_path)) || spn_api_path_rejected(unit, "spn_copy", sp_str_view(to_path))) {
    return SPN_ERROR;
  }
  return spn_api_copy_rooted(unit, from_dir, sp_cstr_as_str(from_path), to_dir, sp_cstr_as_str(to_path));
}

spn_profile_t* spn_get_profile(spn_t* s) {
  spn_pkg_unit_t* unit = sp_ptr_cast(spn_pkg_unit_t*, s);
  return sp_ptr_cast(spn_profile_t*, &unit->build->profile);
}

spn_libc_kind_t spn_profile_get_libc(spn_profile_t* profile) {
  spn_profile_info_t* info = (spn_profile_info_t*)profile;
  switch (info->abi) {
    case SPN_ABI_MUSL: return SPN_LIBC_MUSL;
    default:           return SPN_LIBC_GNU;
  }
}

spn_build_mode_t spn_profile_get_mode(spn_profile_t* profile) {
  return ((spn_profile_info_t*)profile)->mode;
}

spn_opt_level_t spn_profile_get_opt(spn_profile_t* profile) {
  return ((spn_profile_info_t*)profile)->opt;
}

spn_sanitizer_set_t spn_profile_get_sanitizers(spn_profile_t* profile) {
  return ((spn_profile_info_t*)profile)->sanitizers;
}

void spn_target_add_source(spn_target_t* target, const c8* source) {
  spn_path_t made = api_path(target->unit, "spn_target_add_source", source);
  if (spn_path_empty(made)) {
    return;
  }
  sp_da_push(target->info->source, made);
}

void spn_target_add_include(spn_target_t* target, const c8* include) {
  spn_path_t made = api_path(target->unit, "spn_target_add_include", include);
  if (spn_path_empty(made)) {
    return;
  }
  sp_da_push(target->info->include, made);
}

void spn_target_add_define(spn_target_t* target, const c8* define) {
  sp_da_push(target->info->define, spn_intern_cstr(define));
}

void spn_target_add_flag(spn_target_t* target, const c8* flag) {
  sp_da_push(target->info->flags, spn_intern_cstr(flag));
}

// Channel a little bit of Arthur himself to get these wrappers to fit on one line on my editor
#define view(_str) sp_str_view(_str)
#define DATA_T SP_EMBED_DEFAULT_DATA_T_S
#define SIZE_T SP_EMBED_DEFAULT_SIZE_T_S
void spn_target_embed_file(spn_target_t* t, const c8* file) {
  spn_path_t made = api_path(t->unit, "spn_target_embed_file", file);
  if (spn_path_empty(made)) return;
  spn_target_embed_file_ex_s(t->info, made, SP_EMBED_DEFAULT_SYMBOL_S, DATA_T, SIZE_T);
}

void spn_target_embed_file_ex(spn_target_t* t, const c8* f, const c8* s, const c8* d_t, const c8* s_t) {
  spn_path_t made = api_path(t->unit, "spn_target_embed_file_ex", f);
  if (spn_path_empty(made)) return;
  spn_target_embed_file_ex_s(t->info, made, view(s), view(d_t), view(s_t));
}

void spn_target_embed_dir_ex(spn_target_t* t, const c8* d, const c8* dest, const c8* d_t, const c8* s_t) {
  spn_path_t made = api_path(t->unit, "spn_target_embed_dir_ex", d);
  if (spn_path_empty(made) || spn_api_path_rejected(t->unit, "spn_target_embed_dir_ex", view(dest))) return;
  spn_target_embed_dir_ex_s(t->info, made, view(dest), view(d_t), view(s_t));
}
