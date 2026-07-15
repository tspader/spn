#include "forward/types.h"
#include "sp/macro.h"
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
#include "compiler/driver.h"
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

void spn_api_add_profile_flags_env(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, sp_env_t* env) {
  spn_cc_flags_t flags = sp_zero;
  spn_err_union_t err = spn_cc_render_flags(mem, toolchain, profile, &flags);
  sp_assert(!err.kind);
  sp_str_t compile = sp_str_join_n(mem, flags.compile, sp_da_size(flags.compile), sp_str_lit(" "));
  sp_str_t link = sp_str_join_n(mem, flags.link, sp_da_size(flags.link), sp_str_lit(" "));

  sp_env_insert(env, sp_str_lit("CFLAGS"), compile);
  sp_env_insert(env, sp_str_lit("CXXFLAGS"), compile);
  if (!sp_str_empty(link)) {
    sp_env_insert(env, sp_str_lit("LDFLAGS"), link);
  }
}

sp_ps_output_t spn_api_subprocess(sp_mem_t mem, spn_pkg_unit_t* unit, sp_ps_config_t config) {
  SPN_API_LOG(unit, "spn_api_subprocess", "{}", SP_FMT_STR(config.command));

  if (sp_str_empty(config.cwd)) {
    config.cwd = unit->paths.work;
  }

  config.io = (sp_ps_io_config_t) {
    .in =  { .mode = SP_PS_IO_MODE_NULL },
    .out = { .mode = SP_PS_IO_MODE_CREATE },
    .err = { .mode = SP_PS_IO_MODE_REDIRECT },
  };

  // Inherit the process env, then layer the caller's vars and the session's
  // overrides (e.g. the toolchain's CC/AR/LD) on top
  spn_session_t* session = unit->session;
  sp_env_t env = sp_env_capture(mem);
  if (config.env.env.vars) {
    sp_ht_for_kv(config.env.env.vars, it) {
      sp_env_insert(&env, *it.key, *it.val);
    }
  }
  sp_ht_for_kv(session->env.vars, it) {
    if (sp_str_empty(*it.val)) continue;
    sp_env_insert(&env, *it.key, *it.val);
  }
  config.env.env = env;
  config.env.mode = SP_PS_ENV_EXISTING;

  sp_ps_output_t result = sp_ps_run(mem, config);
  if (!sp_str_empty(result.out)) {
    sp_io_write_str(&unit->logs.io.build.writer, result.out, SP_NULLPTR);
  }
  return result;
}

static s32 run_argv(spn_pkg_unit_t* unit, spn_toolchain_launcher_t* launcher, const c8** args) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_config_t config = sp_zero;

  if (launcher) {
    config.command = launcher->program;
    sp_da_for(launcher->args, it) {
      sp_ps_config_add_arg(scratch.mem, &config, launcher->args[it]);
    }
  }
  else {
    config.command = sp_str_view(args[0]);
    args++;
  }

  for (const c8** arg = args; *arg; arg++) {
    sp_ps_config_add_arg(scratch.mem, &config, sp_str_view(*arg));
  }

  sp_ps_output_t result = spn_api_subprocess(scratch.mem, unit, config);
  s32 exit_code = result.status.exit_code;
  sp_mem_end_scratch(scratch);
  return exit_code;
}

static spn_target_t* wrap_target(const void* opaque, spn_target_info_t* info) {
  if (!info) return SP_NULLPTR;

  spn_target_t* target = sp_alloc_type(spn.mem, spn_target_t);
  *target = (spn_target_t) {
    .spn = (spn_t*)opaque,
    .info = info,
  };
  return target;
}

spn_target_t* spn_get_target(spn_t* spn, const c8* name) {
  return wrap_target(spn, spn_pkg_get_target(spn_api_unit(spn)->info, name));
}

spn_target_t* spn_add_exe(spn_config_t* config, const c8* name) {
  return wrap_target(config, spn_pkg_add_exe(spn_api_unit(config)->info, name));
}

spn_target_t* spn_add_lib(spn_config_t* config, const c8* name, spn_linkage_t kind) {
  spn_linkage_set_t linkages = sp_zero;
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
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t joined = sp_fs_join_path(scratch.mem, spn_api_dir(unit, base), sp_str_view(path));
  const c8* result = sp_str_to_cstr(spn.mem, joined);
  sp_mem_end_scratch(scratch);
  return result;
}

void spn_log(spn_t* s, const c8* message) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  spn_event_buffer_push_ex(spn.events, unit->info, &unit->logs.io, (spn_build_event_t) {
    .kind = SPN_EVENT_USER_LOG,
    .user_log = { .message = sp_str_from_cstr(spn.mem, message) },
  });
}

void spn_write_file(spn_t* s, const c8* path, const c8* content) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  SPN_API_LOG(unit, "spn_write_file", "{}", SP_FMT_CSTR(path));

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t full_path = sp_fs_join_path(scratch.mem, unit->paths.work, sp_str_view(path));
  sp_str_t parent = sp_fs_parent_path(full_path);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_io_file_writer_t writer;
  sp_io_file_writer_from_path(&writer, full_path);
  sp_io_write_cstr(&writer.base, content, SP_NULLPTR);
  sp_io_file_writer_close(&writer);
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
          sp_fs_copy(sp_fs_join_path(scratch.mem, dir, entries[it].name), to);
        }
      }
    }
  }
  else if (!sp_fs_exists(from)) {
    err = SPN_ERROR;
  }
  else {
    // @spader This bit me so I just patched it over like this, but
    // I need to think about how this should work
    sp_str_t parent = sp_fs_parent_path(to);
    if (!sp_str_empty(parent)) {
      sp_fs_create_dir(parent);
    }

    err = sp_fs_copy(from, to);
  }

  sp_mem_end_scratch(scratch);
  return err;
}

s32 spn_copy(spn_t* s, spn_dir_t from_dir, const c8* from_path, spn_dir_t to_dir, const c8* to_path) {
  spn_pkg_unit_t* unit = spn_api_unit(s);
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t from = sp_fs_join_path(scratch.mem, spn_api_dir(unit, from_dir), sp_str_view(from_path));
  sp_str_t to = sp_fs_join_path(scratch.mem, spn_api_dir(unit, to_dir), sp_str_view(to_path));
  SPN_API_LOG(unit, "spn_copy", "{} -> {}", SP_FMT_STR(from), SP_FMT_STR(to));

  s32 err = spn_api_copy(from, to);
  sp_mem_end_scratch(scratch);
  return err;
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

spn_linkage_t spn_profile_get_linkage(spn_profile_t* profile) {
  return ((spn_profile_info_t*)profile)->linkage;
}

spn_c_standard_t spn_profile_get_standard(spn_profile_t* profile) {
  return ((spn_profile_info_t*)profile)->standard;
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
  spn_target_embed_dir_ex_s(t->info, view(d), sp_str_lit(""), DATA_T, SIZE_T);
}

void spn_target_embed_dir_ex(spn_target_t* t, const c8* d, const c8* dest, const c8* d_t, const c8* s_t) {
  spn_target_embed_dir_ex_s(t->info, view(d), view(dest), view(d_t), view(s_t));

}
