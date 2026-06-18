#include "sp.h"
#include "spn.h"
#include "sp/sp_glob.h"

#include "api/api.h"
#include "api/core/types.h"
#include "ctx/types.h"
#include "event/types.h"
#include "pkg/types.h"
#include "session/types.h"
#include "target/types.h"
#include "unit/types.h"

#include "event/event.h"
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

sp_str_t spn_api_dir(spn_pkg_unit_t* unit, spn_dir_t dir) {
  switch (dir) {
    case SPN_DIR_NONE:    return sp_str_lit("");
    // Units have no distinct cache dir; CACHE aliases the store
    case SPN_DIR_CACHE:   return unit->paths.store;
    case SPN_DIR_STORE:   return unit->paths.store;
    case SPN_DIR_INCLUDE: return unit->paths.include;
    case SPN_DIR_VENDOR:  return unit->paths.vendor;
    case SPN_DIR_LIB:     return unit->paths.lib;
    case SPN_DIR_SOURCE:  return unit->paths.source;
    case SPN_DIR_WORK:    return unit->paths.work;
    case SPN_DIR_PROJECT: return unit->session->paths.root;
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_ps_output_t spn_api_subprocess(spn_pkg_unit_t* unit, sp_ps_config_t config) {
  SPN_API_LOG(unit, "spn_api_subprocess", "{}", SP_FMT_STR(config.command));

  if (sp_str_empty(config.cwd)) {
    config.cwd = unit->paths.work;
  }

  config.io = (sp_ps_io_config_t) {
    .in =  { .mode = SP_PS_IO_MODE_NULL },
    .out = { .mode = SP_PS_IO_MODE_CREATE },
    .err = { .mode = SP_PS_IO_MODE_REDIRECT },
  };

  // The session env holds only our overrides (e.g. the toolchain's CC/AR/LD);
  // layer them on top of the default SP_PS_ENV_INHERIT. Empty keys are the
  // terminator for extra[], so scan with the same sentinel.
  spn_session_t* session = unit->session;
  u32 slot = 0;
  for (; slot < sp_carr_len(config.env.extra); slot++) {
    if (sp_str_empty(config.env.extra[slot].key)) break;
  }

  sp_ht_for_kv(session->env.vars, it) {
    if (slot >= sp_carr_len(config.env.extra)) break;
    if (sp_str_empty(*it.val)) continue;
    config.env.extra[slot++] = (sp_env_var_t) { .key = *it.key, .value = *it.val };
  }

  sp_ps_output_t result = sp_ps_run(spn_allocator, config);
  if (!sp_str_empty(result.out)) {
    sp_io_write_str(&unit->logs.io.build.writer, result.out, SP_NULLPTR);
  }
  return result;
}

static s32 run_argv(spn_pkg_unit_t* unit, spn_toolchain_launcher_t* launcher, const c8** args) {
  sp_ps_config_t config = SP_ZERO_INITIALIZE();

  if (launcher) {
    config.command = launcher->program;
    sp_da_for(launcher->args, it) {
      sp_ps_config_add_arg(spn_allocator, &config, launcher->args[it]);
    }
  }
  else {
    config.command = sp_str_view(args[0]);
    args++;
  }

  for (const c8** arg = args; *arg; arg++) {
    sp_ps_config_add_arg(spn_allocator, &config, sp_str_view(*arg));
  }

  sp_ps_output_t result = spn_api_subprocess(unit, config);
  return result.status.exit_code;
}

s32 spn_exec(spn_t* s, const c8** args) {
  return run_argv(spn_api_unit(s), SP_NULLPTR, args);
}

s32 spn_cc(spn_t* s, const c8** args) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  return run_argv(unit, &unit->session->units.toolchain->compiler, args);
}

s32 spn_ar(spn_t* s, const c8** args) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  return run_argv(unit, &unit->session->units.toolchain->archiver, args);
}

static spn_target_t* wrap_target(const void* spn, spn_target_info_t* info) {
  if (!info) return SP_NULLPTR;

  spn_target_t* target = sp_alloc_type(spn_allocator, spn_target_t);
  target->spn = (spn_t*)spn;
  target->info = info;
  return target;
}

spn_target_t* spn_get_target(spn_t* spn, const c8* name) {
  return wrap_target(spn, spn_pkg_get_target(spn_api_unit(spn)->info, name));
}

spn_target_t* spn_add_exe(spn_config_t* config, const c8* name) {
  return wrap_target(config, spn_pkg_add_exe(spn_api_unit(config)->info, name));
}

spn_target_t* spn_add_lib(spn_config_t* config, const c8* name, spn_linkage_t kind) {
  spn_linkage_set_t linkages = SP_ZERO_INITIALIZE();
  spn_linkage_set_add(&linkages, kind);
  return wrap_target(config, spn_pkg_add_lib_ex(spn_api_unit(config)->info, spn_intern_cstr(name), linkages));
}

spn_target_t* spn_add_test(spn_config_t* config, const c8* name) {
  return wrap_target(config, spn_pkg_add_test(spn_api_unit(config)->info, name));
}

void spn_add_include(spn_config_t* config, const c8* path) {
  spn_pkg_add_include(spn_api_unit(config)->info, path);
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

  sp_ht_for_kv(unit->info->deps, it) {
    spn_requested_pkg_t* dep = it.val;
    if (!sp_str_equal(spn_qualified_name_to_pkg_id(dep->qualified).name, key)) continue;

    spn_pkg_unit_t* dep_unit = spn_session_find_pkg_by_qualified(unit->session, dep->qualified);
    if (dep_unit) return (const spn_t*)dep_unit;
  }

  return SP_NULLPTR;
}

const c8* spn_get_dir(const spn_t* s, spn_dir_t dir) {
  return sp_str_to_cstr(spn_allocator, spn_api_dir(spn_api_unit(s), dir));
}

const c8* spn_get_subdir(const spn_t* s, spn_dir_t base, const c8* path) {
  return sp_str_to_cstr(spn_allocator, sp_fs_join_path(spn_allocator, spn_api_dir(spn_api_unit(s), base), sp_str_view(path)));
}

void spn_log(spn_t* s, const c8* message) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  spn_event_buffer_push_ex(spn.events, unit->info, &unit->logs.io, (spn_build_event_t) {
    .kind = SPN_EVENT_USER_LOG,
    .user_log = { .message = sp_str_from_cstr(spn_allocator, message) },
  });
}

void spn_write_file(spn_t* s, const c8* path, const c8* content) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  SPN_API_LOG(unit, "spn_write_file", "{}", SP_FMT_CSTR(path));

  sp_str_t full_path = sp_fs_join_path(spn_allocator, unit->paths.work, sp_str_view(path));
  sp_str_t parent = sp_fs_parent_path(full_path);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_io_writer_t* io = sp_io_writer_from_file(full_path, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_cstr(io, content, SP_NULLPTR);
  sp_io_writer_close(io);
}

s32 spn_copy(spn_t* s, spn_dir_t from_dir, const c8* from_path, spn_dir_t to_dir, const c8* to_path) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  sp_str_t from = sp_fs_join_path(spn_allocator, spn_api_dir(unit, from_dir), sp_str_view(from_path));
  sp_str_t to = sp_fs_join_path(spn_allocator, spn_api_dir(unit, to_dir), sp_str_view(to_path));
  SPN_API_LOG(unit, "spn_copy", "{} -> {}", SP_FMT_STR(from), SP_FMT_STR(to));

  // sp_fs_copy only understands a bare "*" or an exact name; expand real glob
  // patterns (e.g. "lib/*.o") ourselves against the source directory.
  sp_str_t pattern = sp_fs_get_name(from);
  if (sp_fs_is_glob(from) && !sp_str_equal(pattern, sp_str_lit("*"))) {
    sp_str_t dir = sp_fs_parent_path(from);
    sp_fs_create_dir(to);

    sp_glob_set_t* glob = sp_glob_set_new(spn_allocator);
    sp_glob_set_add(glob, sp_str_to_cstr(spn_allocator, pattern));
    sp_glob_set_build(glob);

    sp_da(sp_fs_entry_t) entries = sp_fs_collect(spn_allocator, dir);
    sp_da_for(entries, i) {
      if (sp_glob_set_match(glob, entries[i].name)) {
        sp_fs_copy(sp_fs_join_path(spn_allocator, dir, entries[i].name), to);
      }
    }
    return SPN_OK;
  }

  return sp_fs_copy(from, to);
}

spn_profile_t* spn_get_profile(spn_t* s) {
  return (spn_profile_t*)&spn_api_unit(s)->session->profile;
}

spn_libc_kind_t spn_profile_get_libc(spn_profile_t* profile) {
  spn_profile_info_t* info = (spn_profile_info_t*)profile;
  switch (info->abi) {
    case SPN_ABI_MUSL: return SPN_LIBC_MUSL;
    default:           return SPN_LIBC_GNU;
  }
}

spn_linkage_t spn_profile_get_linkage(spn_profile_t* profile) {
  return ((spn_profile_info_t*)profile)->linkage;
}

spn_c_standard_t spn_profile_get_standard(spn_profile_t* profile) {
  return ((spn_profile_info_t*)profile)->standard;
}

spn_build_mode_t spn_profile_get_mode(spn_profile_t* profile) {
  return ((spn_profile_info_t*)profile)->mode;
}

void spn_target_add_source(spn_target_t* target, const c8* source) {
  sp_da_push(target->info->source, spn_intern_cstr(source));
}

void spn_target_add_include(spn_target_t* target, const c8* include) {
  sp_da_push(target->info->include, spn_intern_cstr(include));
}

void spn_target_add_define(spn_target_t* target, const c8* define) {
  sp_da_push(target->info->define, spn_intern_cstr(define));
}

void spn_target_add_flag(spn_target_t* target, const c8* flag) {
  sp_da_push(target->info->flags, spn_intern_cstr(flag));
}

void spn_target_set_linked(spn_target_t* target, s32 linked) {
  target->info->no_link = !linked;
}

// Channel a little bit of Arthur himself to get these wrappers to fit on one line on my editor
#define view(_str) sp_str_view(_str)
#define DATA_T SP_EMBED_DEFAULT_DATA_T_S
#define SIZE_T SP_EMBED_DEFAULT_SIZE_T_S
void spn_target_embed_file(spn_target_t* t, const c8* file) {
  spn_target_embed_file_ex_s(t->info, view(file), SP_EMBED_DEFAULT_SYMBOL_S, DATA_T, SIZE_T);
}

void spn_target_embed_file_ex(spn_target_t* t, const c8* f, const c8* s, const c8* d_t, const c8* s_t) {
  spn_target_embed_file_ex_s(t->info, view(f), view(s), view(d_t), view(s_t));
}

void spn_target_embed_mem(spn_target_t* t, const c8* s, const u8* b, u64 z) {
  spn_target_embed_mem_ex_s(t->info, view(s), b, z, DATA_T, SIZE_T);
}

void spn_target_embed_mem_ex(spn_target_t* t, const c8* s, const u8* b, u64 z, const c8* d_t, const c8* s_t) {
  spn_target_embed_mem_ex_s(t->info, view(s), b, z, view(d_t), view(s_t));
}

void spn_target_embed_dir(spn_target_t* t, const c8* d) {
  spn_target_embed_dir_ex_s(t->info, view(d), DATA_T, SIZE_T);
}

void spn_target_embed_dir_ex(spn_target_t* t, const c8* d, const c8* d_t, const c8* s_t) {
  spn_target_embed_dir_ex_s(t->info, view(d), view(d_t), view(s_t));

}
