// EXTERNAL
#define SP_MAIN
#define SP_IMPLEMENTATION
#include "sp.h"

#define TOML_IMPLEMENTATION
#include "toml.h"

#include "libtcc.h"

// STANDARD
#include <setjmp.h>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif

  #ifndef NOMINMAX
    #define NOMINMAX
  #endif

  #include <windows.h>
  #include <shlobj.h>
  #include <commdlg.h>
  #include <shellapi.h>
  #include <conio.h>
  #include <io.h>
#endif

#if defined(SP_POSIX)
  #include <dlfcn.h>
  #include <signal.h>
  #include <unistd.h>
#endif

#if defined(SP_LINUX)
  #include <unistd.h>
  #include <string.h>
  #include <pty.h>
  #include <sys/wait.h>
#endif

// SPN
#include "spn.h"
#include "autoconf.h"
#include "cc.h"
#include "cmake.h"
#include "ctx.h"
#include "option.h"
#include "lock.h"
#include "make.h"
#include "gen.h"
#include "intern.h"
#include "resolve.h"
#include "registry.h"
#include "target_filter.h"
#include "log.h"
#include "pkg.h"
#include "profile.h"
#include "semver.h"
#include "graph.h"
#include "node.h"
#include "session.h"
#include "ordered_map.h"
#include "pty.h"
#include "spinner.h"
#include "task.h"
#include "tui.h"
#include "event_buffer.h"
#define SPN_CLI_IMPLEMENTATION
#define SPN_CLI_HELP
#include "spn_cli.h"
#include "spn_app.h"
#include "stoml.h"
#include "external/git.h"
#include "external/tcc.h"
#include "sp/color.h"
#include "sp/ht.h"
#include "sp/io.h"
#include "sp/macro.h"
#include "sp/os.h"
#include "sp/ps.h"
#include "sp/str.h"

#define SPN_VERSION "1.0.0"
#define SPN_COMMIT "00c0fa98"

#include "spn.embed.h"

spn_app_t app;
spn_ctx_t spn;



////////////////////
// IMPLEMENTATION //
////////////////////

spn_user_node_t* spn_find_user_node(spn_node_t node) {
  SP_ASSERT(node.index < sp_da_size(node.ctx->nodes.all));
  return &node.ctx->nodes.all[node.index];
}

spn_node_t spn_add_node(spn_build_ctx_t* c, const c8* tag) {
  // @hack
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)c;
  u32 index = sp_da_size(unit->nodes.all);
  spn_user_node_t node = {
    .ctx = unit,
    .tag = spn_intern_cstr(tag),
  };
  sp_da_push(unit->nodes.all, node);

  return (spn_node_t) {
    .ctx = unit,
    .index = index
  };
}

void spn_node_add_input(spn_node_t node, const c8* input) {
  spn_user_node_t* info = spn_find_user_node(node);
  sp_da_push(info->inputs, spn_intern_cstr(input));
}

void spn_node_add_output(spn_node_t node, const c8* output) {
  spn_user_node_t* info = spn_find_user_node(node);
  sp_da_push(info->outputs, spn_intern_cstr(output));
}

void spn_node_link(spn_node_t from, spn_node_t to) {
  spn_user_node_t* info = spn_find_user_node(to);
  sp_da_push(info->deps, from);
}

void spn_node_set_fn(spn_node_t node, spn_node_fn_t fn) {
  spn_user_node_t* info = spn_find_user_node(node);
  info->fn = fn;
}

void spn_node_set_user_data(spn_node_t node, void* user_data) {
  spn_user_node_t* info = spn_find_user_node(node);
  info->user_data = user_data;
}

s32 spn_executor_user_fn(spn_bg_cmd_t* cmd, void* user_data) {
  spn_user_node_t* node = (spn_user_node_t*)user_data;

  spn_event_buffer_push_ctx(spn.events, &node->ctx->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_USER_FN,
    .node = { .info = node }
  });

  spn_err_t err = SPN_OK;
  if (node->fn) {
    spn_node_ctx_t ctx = {
      .build = &node->ctx->ctx,
      .user_data = node->user_data
    };

    switch (node->fn(&ctx)) {
      case SPN_OK: {
        spn_pkg_unit_write_stamp(node->ctx, spn_pkg_unit_get_node_stamp_file(node->ctx, node));
        sp_str_t stamp = sp_fs_join_path(node->ctx->paths.stamp.dir, node->tag);
        sp_fs_create_file(stamp);
        break;
      }
      default: {
        break;
      }
    }
  }
  return err;
}

spn_pkg_t* spn_get_pkg(spn_build_ctx_t* b) {
  return b->pkg;
}

spn_profile_t* spn_get_profile(spn_build_ctx_t* b) {
  return b->profile;
}

spn_target_t* spn_get_target(spn_build_ctx_t* b, const c8* name) {
  return spn_pkg_get_target(b->pkg, name);
}

const spn_build_ctx_t* spn_get_dep(spn_build_ctx_t* b, const c8* name) {
  spn_pkg_unit_t* unit = sp_om_get(b->session->units.packages, spn_intern_cstr(name));
  return &unit->ctx;
}

const c8* spn_get_dir(const spn_build_ctx_t* b, spn_pkg_dir_t kind) {
  return sp_str_to_cstr(spn_build_ctx_get_dir(b, kind));
}

const c8* spn_get_subdir(const spn_build_ctx_t* b, spn_pkg_dir_t kind, const c8* path) {
  sp_str_t result = sp_fs_join_path(spn_build_ctx_get_dir(b, kind), sp_str_view(path));
  return sp_str_to_cstr(result);
}

spn_target_t* spn_add_exe(spn_config_t* c, const c8* name) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)c;
  spn_target_t* target = spn_pkg_add_exe(unit->ctx.pkg, name);
  spn_pkg_unit_add_target(unit, target);
  return target;
}

spn_target_t* spn_add_test(spn_config_t* c, const c8* name) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)c;
  spn_target_t* target = spn_pkg_add_test(unit->ctx.pkg, name);
  spn_pkg_unit_add_target(unit, target);
  return target;
}

spn_target_t* spn_add_lib(spn_config_t* c, const c8* name, spn_linkage_t kind) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)c;
  spn_target_t* target = spn_pkg_add_lib(unit->ctx.pkg, name, kind);
  spn_pkg_unit_add_target(unit, target);
  return target;
}

void spn_log(spn_build_ctx_t* ctx, const c8* message) {
  spn_build_ctx_log(&ctx->logs, sp_str_view(message));
}

void spn_copy(spn_build_ctx_t* build, spn_pkg_dir_t from_kind, const c8* from_path, spn_pkg_dir_t to_kind, const c8* to_path) {
  sp_str_t from = sp_fs_join_path(spn_build_ctx_get_dir(build, from_kind), sp_str_view(from_path));
  sp_str_t to = sp_fs_join_path(spn_build_ctx_get_dir(build, to_kind), sp_str_view(to_path));
  sp_fs_copy(from, to);
}

void spn_write_file(spn_build_ctx_t* build, const c8* path, const c8* content) {
  sp_str_t full_path = sp_fs_join_path(spn_build_ctx_get_dir(build, SPN_DIR_WORK), sp_str_view(path));
  sp_str_t parent = sp_fs_parent_path(full_path);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }
  sp_io_writer_t io = sp_io_writer_from_file(full_path, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_cstr(&io, content);
  sp_io_writer_close(&io);
}


///////////
// ENUMS //
///////////
sp_str_t spn_cli_opt_kind_to_str(spn_cli_opt_kind_t kind) {
  switch (kind) {
    SPN_CLI_OPT_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_cli_arg_kind_t spn_cli_arg_kind_from_str(sp_str_t str) {
  SPN_CLI_ARG_KIND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_CLI_ARG_KIND_REQUIRED);
}

sp_str_t spn_cli_arg_kind_to_str(spn_cli_arg_kind_t kind) {
  switch (kind) {
    SPN_CLI_ARG_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_cli_cmd_t spn_cli_command_from_str(sp_str_t str) {
  SPN_CLI_COMMAND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_CLI_LS);
}

sp_str_t spn_cli_command_to_str(spn_cli_cmd_t cmd) {
  switch (cmd) {
    SPN_CLI_COMMAND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_tool_cmd_t spn_tool_subcommand_from_str(sp_str_t str) {
  SPN_TOOL_SUBCOMMAND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_TOOL_LIST);
}

sp_str_t spn_tool_subcommand_to_str(spn_tool_cmd_t cmd) {
  switch (cmd) {
    SPN_TOOL_SUBCOMMAND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_cache_dir_kind_to_path(spn_pkg_dir_t kind) {
  switch (kind) {
    case SPN_DIR_PROJECT: return spn.paths.project;
    case SPN_DIR_CACHE:   return spn.paths.cache;
    case SPN_DIR_STORE:   return spn.paths.store;
    case SPN_DIR_SOURCE:  return spn.paths.source;
    case SPN_DIR_WORK:    return spn.paths.cwd;
    default: SP_UNREACHABLE_RETURN(sp_str_lit(""));
  }
}
sp_str_t spn_build_ctx_resolve_dir(const spn_build_ctx_t* b, spn_pkg_dir_t kind, sp_str_t sub) {
  return sp_fs_join_path(spn_build_ctx_get_dir(b, kind), sub);
}

sp_str_t spn_build_ctx_get_dir(const spn_build_ctx_t* b, spn_pkg_dir_t kind) {
  sp_str_t result = SP_ZERO_INITIALIZE();
  switch (kind) {
    case SPN_DIR_STORE:   return b->paths.store;
    case SPN_DIR_INCLUDE: return b->paths.include;
    case SPN_DIR_LIB:     return b->paths.lib;
    case SPN_DIR_VENDOR:  return b->paths.vendor;
    case SPN_DIR_SOURCE:  return b->paths.source;
    case SPN_DIR_WORK:    return b->paths.work;
    case SPN_DIR_CACHE:   return b->paths.store;
    case SPN_DIR_PROJECT: return spn.paths.project;
    case SPN_DIR_NONE:    return sp_str_lit("");
  }

  SP_UNREACHABLE();
  return sp_str_lit("");
}

sp_str_t spn_build_ctx_get_include_dir(spn_build_ctx_t* build) {
  return build->paths.include;
}

sp_str_t spn_app_project_dir(void) {
  return app.paths.dir;
}

sp_str_t spn_pkg_unit_get_include_dir(spn_pkg_unit_t* unit) {
  return spn_build_ctx_get_include_dir(&unit->ctx);
}

sp_intern_t* spn_ctx_get_intern(void) {
  return spn.intern;
}

spn_log_level_t spn_ctx_get_log_level(void) {
  return spn.log_level;
}

sp_io_writer_t* spn_ctx_get_log_out(void) {
  return &spn.logger.out;
}

sp_io_writer_t* spn_ctx_get_log_err(void) {
  return &spn.logger.err;
}

sp_str_t spn_ctx_source_cache_root(void) {
  return spn.paths.source;
}

sp_str_t spn_ctx_build_cache_root(void) {
  return spn.paths.build;
}

sp_str_t spn_ctx_store_cache_root(void) {
  return spn.paths.store;
}

sp_str_t spn_ctx_project_root(void) {
  return spn.paths.project;
}

sp_str_t spn_ctx_build_source_dir(spn_build_ctx_t* build) {
  return build->paths.source;
}

sp_str_t spn_ctx_build_work_dir(spn_build_ctx_t* build) {
  return build->paths.work;
}

sp_str_t spn_ctx_build_store_dir(spn_build_ctx_t* build) {
  return build->paths.store;
}

sp_str_t spn_ctx_build_include_dir(spn_build_ctx_t* build) {
  return build->paths.include;
}

sp_str_t spn_ctx_build_lib_dir(spn_build_ctx_t* build) {
  return build->paths.lib;
}

spn_linkage_t spn_ctx_build_linkage(spn_build_ctx_t* build) {
  return build->linkage;
}

spn_build_mode_t spn_ctx_build_mode(spn_build_ctx_t* build) {
  return build->profile->mode;
}

sp_ps_output_t spn_ctx_build_subprocess(spn_build_ctx_t* build, sp_ps_config_t cfg) {
  return spn_build_ctx_subprocess(build, cfg);
}

sp_da(sp_str_t) spn_ctx_build_lib_entries(spn_build_ctx_t* build) {
  sp_da(sp_str_t) entries = SP_NULLPTR;
  sp_om_for(build->pkg->libs, it) {
    spn_target_t* lib = sp_om_at(build->pkg->libs, it);
    sp_da_push(entries, spn_build_ctx_get_lib_path(build, lib));
  }
  return entries;
}

sp_da(spn_build_ctx_t*) spn_ctx_all_build_contexts(void) {
  sp_da(spn_build_ctx_t*) builds = SP_NULLPTR;
  sp_om_for(app.session.units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(app.session.units.packages, it);
    sp_da_push(builds, &unit->ctx);
  }
  return builds;
}

spn_pkg_t* spn_ctx_ensure_package(spn_pkg_req_t req) {
  return spn_app_ensure_package(&app, req);
}

void spn_ctx_bail_on_missing_package(sp_str_t name) {
  spn_app_bail_on_missing_package(&app, name);
}

spn_pkg_t* spn_ctx_root_package(void) {
  return &app.package;
}

spn_resolver_t* spn_ctx_resolver(void) {
  return &app.resolver;
}

void spn_ctx_push_target_source_event(spn_target_t* target, sp_str_t source) {
  spn_event_buffer_push_ex(spn.events, target->pkg, SP_NULLPTR, (spn_build_event_t) {
    .kind = SPN_EVENT_ADD_SOURCE,
    .target_source = {
      .target = target->name,
      .source = source,
    }
  });
}

sp_str_t spn_build_ctx_get_lib_dir(spn_build_ctx_t* build) {
  switch (build->linkage) {
    case SPN_LIB_KIND_SHARED: {
      return build->paths.lib;
      break;
    }
    case SPN_LIB_KIND_STATIC:
    case SPN_LIB_KIND_SOURCE: {
      return SP_ZERO_STRUCT(sp_str_t);
    }
  }
  SP_UNREACHABLE_RETURN(SP_ZERO_STRUCT(sp_str_t));
}

sp_str_t spn_build_ctx_get_rpath(spn_build_ctx_t* build) {
  return spn_build_ctx_get_lib_dir(build);
}

sp_str_t spn_build_ctx_get_lib_path(spn_build_ctx_t* build, spn_target_t* lib_target) {
  spn_linkage_t linkage = spn_target_kind_to_pkg_linkage(lib_target->kind);
  sp_os_lib_kind_t os_kind = spn_lib_kind_to_sp_os_lib_kind(linkage);
  sp_str_t lib = lib_target->name;
  lib = sp_os_lib_to_file_name(lib, os_kind);
  lib = sp_fs_join_path(build->paths.lib, lib);
  return lib;
}

void sp_sh_ls(sp_str_t path) {
  if (!sp_fs_exists(path)) {
    SP_LOG("{:fg brightcyan} hasn't been built for your configuration", SP_FMT_STR(path));
    return;
  }

  struct {
    const c8* command;
    const c8* args [4];
  } tools [4] = {
    { "lsd", "--tree", "--depth", "2" },
    { "tree", "-L", "2" },
    { "ls" },
  };

  SP_CARR_FOR(tools, i) {
    if (sp_fs_is_on_path(sp_str_view(tools[i].command))) {
      sp_ps_config_t config = SP_ZERO_INITIALIZE();
      config.command = sp_str_view(tools[i].command);

      SP_CARR_FOR(tools[i].args, j) {
        const c8* arg = tools[i].args[j];
        if (!arg) break;
        sp_ps_config_add_arg(&config, sp_str_view(arg));
      }

      sp_ps_config_add_arg(&config, path);

      sp_ps_output_t result = sp_ps_run(config);
      SP_ASSERT(!result.status.exit_code);
      SP_LOG("{}", SP_FMT_STR(sp_str_trim(result.out)));
      return;
    }
  }
}


///////////
// BUILD //
///////////
bool sp_cmp_kernel_env_var(void* va, void* vb) {
  sp_env_var_t* a = (sp_env_var_t*)a;
  sp_env_var_t* b = (sp_env_var_t*)b;
  if (!sp_str_equal(a->key, b->key)) return false;
  return sp_str_equal(a->value, b->value);
}

bool sp_zcmp_kernel_env_var(void* va) {
  sp_env_var_t* a = (sp_env_var_t*)a;
  return !sp_str_valid(a->key) && !sp_str_valid(a->value);
}

sp_ps_output_t spn_build_ctx_subprocess(spn_build_ctx_t* build, sp_ps_config_t config) {
  config.io = (sp_ps_io_config_t) {
    .in = { .mode = SP_PS_IO_MODE_NULL },
    .out = { .mode = SP_PS_IO_MODE_EXISTING, .fd = build->logs.build.file.fd },
    .err = { .mode = SP_PS_IO_MODE_REDIRECT }
  };
  config.cwd = build->paths.work;

  u32 it = 0;
  for (; it < sp_carr_len(config.env.extra); it++) {
    if (!sp_str_valid(config.env.extra[it].key)) {
      break;
    }
  }
  SP_ASSERT(it != sp_carr_len(config.env.extra));

  config.env.extra[it] = (sp_env_var_t) {
    .key = sp_str_lit("CC"),
    .value = build->profile->cc.exe
  };

  sp_da_push(build->commands, sp_ps_config_copy(&config));

  sp_ps_t ps = sp_ps_create(config);
  return sp_ps_output(&ps);
}

sp_ps_output_t spn_bg_subprocess(spn_build_io_t* logs, spn_build_paths_t* paths, sp_ps_config_t config) {
  config.io = (sp_ps_io_config_t) {
    .in = { .mode = SP_PS_IO_MODE_NULL },
    .out = { .mode = SP_PS_IO_MODE_EXISTING, .fd = logs->build.file.fd },
    .err = { .mode = SP_PS_IO_MODE_REDIRECT }
  };
  config.cwd = paths->work;

  u32 it = 0;
  for (; it < sp_carr_len(config.env.extra); it++) {
    if (!sp_str_valid(config.env.extra[it].key)) {
      break;
    }
  }
  SP_ASSERT(it != sp_carr_len(config.env.extra));

  sp_env_insert(&config.env.env, sp_str_lit("CC"), sp_str_lit("clang"));
  //
  // sp_da_push(build->commands, sp_ps_config_copy(&config));

  sp_ps_t ps = sp_ps_create(config);
  return sp_ps_output(&ps);
}

sp_str_t spn_get_tool_path(spn_target_t* bin) {
  return sp_fs_join_path(spn.paths.bin, bin->name);
}

void spn_add_include(spn_build_ctx_t* b, spn_pkg_dir_t dir, const c8* path) {
  spn_pkg_add_include_ex(b->pkg, spn_build_ctx_resolve_dir(b, dir, sp_str_view(path)));
}

void spn_add_define(spn_build_ctx_t* b, const c8* define) {
  spn_pkg_add_define(b->pkg, define);
}

void spn_add_system_dep(spn_build_ctx_t* b, const c8* dep) {
  spn_pkg_add_system_dep(b->pkg, dep);
}

void spn_add_linkage(spn_build_ctx_t* b, spn_linkage_t linkage) {
  spn_pkg_add_linkage(b->pkg, linkage);
}

spn_registry_t* spn_add_registry(spn_build_ctx_t* b, const c8* name, const c8* location) {
  return spn_pkg_add_registry(b->pkg, name, location);
}

void spn_app_bail_on_missing_package(spn_app_t* app, sp_str_t name) {
  sp_str_t prefix = sp_str_lit("  > ");
  sp_str_t color = sp_str_lit("brightcyan");

  sp_da(sp_str_t) search = app->search;
  search = sp_str_map(search, sp_dyn_array_size(search), &color, sp_str_map_kernel_colorize);
  search = sp_str_map(search, sp_dyn_array_size(search), &prefix, sp_str_map_kernel_prepend);

  SP_FATAL(
    "Could not find {:fg yellow} on search path: \n{}",
    SP_FMT_STR(name),
    SP_FMT_STR(sp_str_join_n(search, sp_dyn_array_size(search), sp_str_lit("\n")))
  );
}

spn_err_t spn_app_add_pkg_constraints(spn_app_t* app, spn_pkg_t* pkg) {
  spn_resolver_t* resolver = &app->resolver;

  if (sp_ht_key_exists(resolver->visited, pkg->name)) {
    spn_push_event_ex((spn_build_event_t) {
      .kind = SPN_EVENT_ERR_CIRCULAR_DEP,
      .circular = {
        .pkg = pkg
      }
    });

    return SPN_ERROR;
  }

  // system deps
  sp_dyn_array_for(pkg->system_deps, i) {
    sp_str_t sys_dep = pkg->system_deps[i];
    bool found = false;
    sp_dyn_array_for(resolver->system_deps, j) {
      if (sp_str_equal(resolver->system_deps[j], sys_dep)) { found = true; break; }
    }
    if (!found) sp_dyn_array_push(resolver->system_deps, sys_dep);
  }

  // prevent circular deps by marking this dep until we're done with the subtree
  sp_ht_insert(resolver->visited, pkg->name, true);

  sp_ht_for_kv(pkg->deps, it) {
    spn_pkg_req_t request = *it.val;
    sp_require_as(!sp_str_empty(request.name), SPN_ERROR);

    spn_pkg_t* dep = spn_app_ensure_package(app, request);
    if (!dep) {
      spn_push_event_ex((spn_build_event_t) {
        .kind = SPN_EVENT_ERR_UNKNOWN_PKG,
        .unknown = {
          .request = request
        }
      });

      return SPN_ERROR;
    }

    // recurse
    sp_try(spn_app_add_pkg_constraints(app, dep));

    // add the dependency itself
    if (!sp_ht_key_exists(resolver->ranges, dep->name)) {
      sp_ht_insert(resolver->ranges, dep->name, SP_NULLPTR);
    }
    sp_da(spn_resolve_range_t)* ranges = sp_ht_getp(resolver->ranges, dep->name);

    // collect the range of versions which satisfy the request
    spn_resolve_range_t range = {
      .source = request
    };

    switch (request.kind) {
      case SPN_PACKAGE_KIND_FILE: {
        u32 num_versions = sp_dyn_array_size(dep->versions);
        if (num_versions != 1) {
          SP_FATAL(
            "Local dependency {:fg brightcyan} has {} versions",
            SP_FMT_STR(dep->name),
            SP_FMT_U32(num_versions)
          );
        }
        sp_opt_set(range.low, 0);
        sp_opt_set(range.high, 0);

        break;
      }
      case SPN_PACKAGE_KIND_INDEX: {
        spn_semver_t low = request.range.low.version;
        spn_semver_t high = request.range.high.version;

        sp_dyn_array_for(dep->versions, n) {
          spn_semver_t version = dep->versions[n];

          if (!range.low.some) {
            if (spn_semver_satisfies(version, low, request.range.low.op)) {
              sp_opt_set(range.low, n);
            }
          }

          if (spn_semver_satisfies(version, high, request.range.high.op)) {
            sp_opt_set(range.high, n);
          }
        }

        break;
      }
      case SPN_PACKAGE_KIND_ROOT:
      case SPN_PACKAGE_KIND_WORKSPACE: {
        SP_BROKEN();
        break;
      }
      case SPN_PACKAGE_KIND_NONE: {
        SP_UNREACHABLE_CASE();
      }
    }

    sp_dyn_array_push(*ranges, range);
  }

  sp_ht_erase(resolver->visited, pkg->name);

  return SPN_OK;
}

void spn_app_resolve_from_lock_file(spn_app_t* app) {
  spn_resolver_init(&app->resolver, &app->package);
  SP_ASSERT(app->lock.some);

  spn_lock_file_t* lock = &app->lock.value;
  sp_ht_for_kv(lock->entries, it) {
    spn_lock_entry_t* entry = it.val;

    spn_pkg_req_t request = {
      .name = *it.key,
      .kind = entry->kind,
      .visibility = entry->visibility,
    };

    if (request.kind == SPN_PACKAGE_KIND_INDEX) {
      request.range = (spn_semver_range_t) {
        .low = { .version = entry->version, .op = SPN_SEMVER_OP_EQ },
        .high = { .version = entry->version, .op = SPN_SEMVER_OP_EQ },
        .mod = SPN_SEMVER_MOD_CMP
      };
    }
    else if (request.kind == SPN_PACKAGE_KIND_FILE) {
      spn_pkg_req_t* dep = sp_ht_getp(app->package.deps, request.name);
      SP_ASSERT(dep);
      SP_ASSERT(dep->kind == SPN_PACKAGE_KIND_FILE);
      request.file = dep->file;
    }

    spn_pkg_t* pkg = spn_app_ensure_package(app, request);

    sp_ht_insert(app->resolver.resolved, entry->name, ((spn_resolved_pkg_t) {
      .pkg = pkg,
      .kind = request.kind,
      .version = entry->version
    }));
  }

  sp_ht_for_kv(lock->system_deps, it) {
    sp_da_push(app->resolver.system_deps, *it.key);
  }
}

spn_err_t spn_app_resolve_from_solver(spn_app_t* app) {
  spn_resolver_init(&app->resolver, &app->package);
  sp_try(spn_app_add_pkg_constraints(app, &app->package));

  sp_ht_for_kv(app->resolver.ranges, it) {
    sp_str_t name = *it.key;
    sp_da(spn_resolve_range_t) ranges = *it.val;
    if (sp_da_empty(ranges)) {
      return SPN_ERROR;
    }

    spn_pkg_req_t req_low, req_high = SP_ZERO_INITIALIZE();
    u32 low = 0, high = SP_LIMIT_U32_MAX;
    sp_dyn_array_for(ranges, n) {
      spn_resolve_range_t range = ranges[n];
      SP_ASSERT(range.low.some);
      SP_ASSERT(range.high.some);

      if (sp_opt_get(range.low) >= low) {
        low = sp_opt_get(range.low);
        req_low = range.source;
      }
      if (sp_opt_get(range.high) <= high) {
        high = sp_opt_get(range.high);
        req_high = range.source;
      }
    }

    if (low > high) {
      sp_str_builder_t builder = SP_ZERO_INITIALIZE();
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} cannot be resolved:", SP_FMT_STR(name));
      sp_str_builder_indent(&builder);
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} requires {:fg brightred}", SP_FMT_STR(req_low.name), SP_FMT_STR(spn_semver_range_to_str(req_low.range)));
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} requires {:fg brightred}", SP_FMT_STR(req_high.name), SP_FMT_STR(spn_semver_range_to_str(req_high.range)));

      SP_FATAL("{}", SP_FMT_STR(sp_str_builder_to_str(&builder)));
    }


    spn_pkg_t* pkg = spn_app_ensure_package(app, req_high);
    sp_ht_insert(app->resolver.resolved, name, ((spn_resolved_pkg_t) {
      .pkg = pkg,
      .version = pkg->versions[high],
      .kind = req_high.kind,
    }));
  }

  return SPN_OK;
}

void spn_app_resolve(spn_app_t* app) {
  switch (app->lock.some) {
    case SP_OPT_SOME: {
      spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(&app->session)->ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_RESOLVE,
        .resolve = {
          .strategy = SPN_RESOLVE_STRATEGY_LOCK_FILE
        }
      });

      spn_app_resolve_from_lock_file(app);
      break;
    }
    case SP_OPT_NONE: {
      spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(&app->session)->ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_RESOLVE,
        .resolve = {
          .strategy = SPN_RESOLVE_STRATEGY_SOLVER
        }
      });

      spn_app_resolve_from_solver(app);
      break;
    }
  }
}

void spn_pkg_unit_init(spn_pkg_unit_t* ctx, spn_pkg_unit_config_t config) {
  spn_build_ctx_init(&ctx->ctx, config.ctx);
  ctx->metadata = config.metadata;
  ctx->paths.stamp.dir = sp_fs_join_path(ctx->ctx.paths.generated, SP_LIT("stamp"));
  ctx->paths.stamp.main = sp_fs_join_path(ctx->paths.stamp.dir, SP_LIT("main.stamp"));
  ctx->paths.stamp.exit = sp_fs_join_path(ctx->paths.stamp.dir, SP_LIT("user.stamp"));
  ctx->paths.stamp.configure = sp_fs_join_path(ctx->paths.stamp.dir, SP_LIT("configure.stamp"));
  ctx->paths.stamp.build = sp_fs_join_path(ctx->paths.stamp.dir, SP_LIT("build.stamp"));
  ctx->paths.stamp.package = sp_fs_join_path(ctx->paths.stamp.dir, SP_LIT("package.stamp"));

  sp_fs_create_dir(ctx->paths.stamp.dir);
}

spn_err_t spn_pkg_unit_sync_remote(spn_pkg_unit_t* build) {
  if (!sp_fs_exists(build->ctx.paths.source)) {
    sp_str_t url = spn_pkg_get_url(build->ctx.pkg);
    sp_try(spn_git_clone(url, build->ctx.paths.source));
  }
  else {
    sp_try(spn_git_fetch(build->ctx.paths.source));
  }

  return SPN_OK;
}

spn_err_t spn_pkg_unit_sync_local(spn_pkg_unit_t* dep) {
  return spn_git_checkout(dep->ctx.paths.source, dep->metadata.commit);
}

void spn_pkg_unit_write_stamp(spn_pkg_unit_t* ctx, sp_str_t path) {
  sp_io_writer_t io = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_OVERWRITE);

  spn_toml_writer_t writer = spn_toml_writer_new();
  spn_toml_begin_table_cstr(&writer, "package");
  spn_toml_append_str_cstr(&writer, "name", ctx->ctx.pkg->name);
  spn_toml_append_str_cstr(&writer, "source", ctx->ctx.paths.source);
  spn_toml_append_str_cstr(&writer, "work", ctx->ctx.paths.work);
  spn_toml_end_table(&writer);

  spn_toml_begin_table_cstr(&writer, "profile");
  spn_toml_append_str_cstr(&writer, "name", ctx->ctx.profile->name);
  spn_toml_append_str_cstr(&writer, "cc", ctx->ctx.profile->cc.exe);
  spn_toml_append_str_cstr(&writer, "linkage", spn_pkg_linkage_to_str(ctx->ctx.profile->linkage));
  spn_toml_append_str_cstr(&writer, "libc", spn_libc_kind_to_str(ctx->ctx.profile->libc));
  spn_toml_append_str_cstr(&writer, "mode", spn_dep_build_mode_to_str(ctx->ctx.profile->mode));
  spn_toml_append_str_cstr(&writer, "standard", spn_c_standard_to_str(ctx->ctx.profile->standard));
  spn_toml_end_table(&writer);

  spn_toml_begin_array_cstr(&writer, "command");
  sp_da_for(ctx->ctx.commands, it) {
    sp_ps_config_t command = ctx->ctx.commands[it];
    spn_toml_append_array_table(&writer);
    spn_toml_append_str(&writer, sp_str_lit("command"), command.command);

    // Add arguments as an array if there are any
    u32 num_args = 0;
    for (; num_args < sp_carr_len(command.args); num_args++) {
      if (!sp_str_valid(command.args[num_args])) {
        break;
      }
    }
    if (num_args) {
      spn_toml_append_str_carr_cstr(&writer, "args", command.args, num_args);
    }


    bool has_env = !sp_ht_empty(command.env.env.vars);
    bool has_extra = sp_str_valid(command.env.extra[0].key);
    if (has_env || has_extra) {
      spn_toml_begin_table_cstr(&writer, "env");

      sp_ht_for_kv(command.env.env.vars, it) {
        spn_toml_append_str(&writer, *it.key, *it.val);
      }

      sp_carr_for(command.env.extra, it) {
        sp_env_var_t var = command.env.extra[it];
        if (sp_str_empty(var.key)) break;
        spn_toml_append_str(&writer, var.key, var.value);
      }

      spn_toml_end_table(&writer);
    }
  }

  spn_toml_end_array(&writer);

  sp_io_write_str(&io, spn_toml_writer_write(&writer));
  sp_io_writer_close(&io);
}

spn_build_ctx_t spn_build_ctx_make(spn_build_ctx_config_t config) {
  spn_build_ctx_t ctx = SP_ZERO_INITIALIZE();
  spn_build_ctx_init(&ctx, config);
  return ctx;
}

void spn_build_ctx_init(spn_build_ctx_t* ctx, spn_build_ctx_config_t config) {
  ctx->arena = sp_mem_arena_new_ex(256, SP_MEM_ARENA_MODE_NO_REALLOC, 1);

  ctx->name = sp_str_copy(config.name);
  ctx->profile = config.session->profile;
  ctx->linkage = config.linkage;
  ctx->pkg = config.package;
  ctx->session = config.session;
  ctx->paths.source = config.paths.source;
  ctx->paths.store = config.paths.store;
  ctx->paths.include = sp_fs_join_path(ctx->paths.store, SP_LIT("include"));
  ctx->paths.bin = sp_fs_join_path(ctx->paths.store, SP_LIT("bin"));
  ctx->paths.lib = sp_fs_join_path(ctx->paths.store, SP_LIT("lib"));
  ctx->paths.vendor = sp_fs_join_path(ctx->paths.store, SP_LIT("vendor"));

  ctx->paths.work = config.paths.work;
  ctx->paths.generated = sp_fs_join_path(ctx->paths.work, SP_LIT("spn"));

  ctx->paths.logs.build = sp_fs_join_path(ctx->paths.work, spn_build_ctx_get_build_log_name(ctx));
  ctx->paths.logs.test = sp_fs_join_path(ctx->paths.work, spn_build_ctx_get_test_log_name(ctx));

  sp_fs_create_dir(ctx->paths.work);
  sp_fs_create_dir(ctx->paths.generated);
  sp_fs_create_dir(ctx->paths.store);
  sp_fs_create_dir(ctx->paths.bin);
  sp_fs_create_dir(ctx->paths.include);
  sp_fs_create_dir(ctx->paths.lib);
  sp_fs_create_dir(ctx->paths.vendor);
  sp_fs_create_file(ctx->paths.logs.build);

  ctx->logs.build = sp_io_writer_from_file(ctx->paths.logs.build, SP_IO_WRITE_MODE_APPEND);
}

void spn_build_ctx_log(spn_build_io_t* logs, sp_str_t message) {
  sp_io_writer_t* io = &logs->build;
  sp_str_builder_t builder;
  sp_io_write_str(io, sp_tm_epoch_to_iso8601(sp_tm_now_epoch()));
  sp_io_write_cstr(io, " [info] ");
  sp_io_write_str(io, message);
  sp_io_write_new_line(io);
}

sp_str_t spn_build_ctx_get_build_log_name(spn_build_ctx_t* build) {
  return sp_format("{}.build.log", SP_FMT_STR(build->name));
}

sp_str_t spn_build_ctx_get_test_log_name(spn_build_ctx_t* build) {
  return sp_format("{}.test.log", SP_FMT_STR(build->name));
}

void register_jit_code(const char *elf_data, size_t elf_size) {
  // struct jit_code_entry *entry = sp_alloc_type(struct jit_code_entry);
  // entry->symfile_addr = elf_data;
  // entry->symfile_size = elf_size;
  //
  // entry->next_entry = __jit_debug_descriptor.first_entry;
  // entry->prev_entry = NULL;
  // if (entry->next_entry)
  //   entry->next_entry->prev_entry = entry;
  //
  // __jit_debug_descriptor.first_entry = entry;
  // __jit_debug_descriptor.relevant_entry = entry;
  // __jit_debug_descriptor.action_flag = JIT_REGISTER_FN;
  //
  // __jit_debug_register_code();
}

spn_err_t spn_session_compile_pkg(spn_session_t* session, spn_pkg_unit_t* unit) {
  spn_pkg_t* pkg = unit->ctx.pkg;

  if (!sp_fs_exists(pkg->paths.script)) {
    return SPN_OK;
  }

  spn_event_buffer_push(spn.events, &unit->ctx, SPN_EVENT_BUILD_SCRIPT_COMPILE);

  sp_tm_timer_t timer = sp_tm_start_timer();
  spn_tcc_err_ctx_t error_context = {
    .arena = unit->ctx.arena,
    .error = sp_str_lit("")
  };

  spn_tcc_t* tcc = tcc_new();
  tcc_set_error_func(tcc, &error_context, spn_tcc_on_build_script_compile_error);
  tcc_set_backtrace_func(tcc, &error_context, spn_tcc_backtrace);
  tcc_set_lib_path(tcc, sp_str_to_cstr(spn.paths.runtime));
  tcc_set_options(tcc, "-gdwarf -Wall -Werror");
  tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);
  tcc_add_include_path(tcc, sp_str_to_cstr(spn.paths.include));
  tcc_define_symbol(tcc, "SPN", "");
  sp_try_goto(spn_tcc_register(tcc), fail);
  sp_try_goto(tcc_add_include_path(tcc, sp_str_to_cstr(spn.paths.include)), fail);
  sp_try_goto(spn_tcc_register(tcc), fail);

  spn_cc_t cc = SP_ZERO();
  spn_cc_set_profile(&cc, session->profile);
  spn_cc_target_t* target = spn_cc_add_target(&cc, SPN_TARGET_JIT, pkg->name);
  sp_ht_for_kv(pkg->deps, it) {
    switch (it.val->visibility) {
      case SPN_VISIBILITY_BUILD: {
        spn_cc_target_add_dep(target, spn_session_find_pkg(session, *it.key));
        break;
      }
      case SPN_VISIBILITY_TEST:
      case SPN_VISIBILITY_PUBLIC: {
        break;
      }
    }
    if (it.val->visibility == SPN_VISIBILITY_BUILD) {
    }
  }

  spn_cc_target_to_tcc(&cc, target, tcc);
  sp_try_goto(spn_tcc_add_file(tcc, pkg->paths.script), fail);
  sp_try_goto(tcc_relocate(tcc), fail);

  unit->tcc = tcc;
  unit->on_configure = tcc_get_symbol(tcc, "configure");
  unit->on_package = tcc_get_symbol(tcc, "package");
  sp_assert_fmt(!tcc_get_symbol(tcc, "build"), "{} still has build()", SP_FMT_STR(unit->ctx.name));

  unit->time.compile = sp_tm_read_timer(&timer);

  return SPN_OK;

fail:
  spn_event_buffer_push_ctx(spn.events, &unit->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED,
    .compile_failed = {
      .error = error_context.error
    }
  });
  return SPN_ERROR;
}

spn_err_t spn_pkg_unit_call_hook(spn_pkg_unit_t* ctx, spn_build_fn_t fn) {
  jmp_buf jump;
  int status = tcc_setjmp(ctx->tcc, jump, fn);
  if (!status) {
    fn(&ctx->ctx); // @spader @refactor
  }
  else {
    spn_event_buffer_push(spn.events, &ctx->ctx, SPN_EVENT_BUILD_SCRIPT_FAILED);
    return SPN_ERROR;
  }

  return SPN_OK;
}



spn_err_t spn_pkg_unit_run_configure_hook(spn_pkg_unit_t* unit) {
  spn_err_t result = SPN_OK;

  spn_event_buffer_push(spn.events, &unit->ctx, SPN_EVENT_BUILD_SCRIPT_CONFIGURE);

  spn_event_buffer_push_ctx(spn.events, &unit->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_RUN_CONFIGURE,
    .configure = {
      .exists = unit->on_configure,
      .result = 0,
      .time = 0,
    }
  });

  if (unit->on_configure) {
    sp_tm_timer_t timer = sp_tm_start_timer();
    result = spn_pkg_unit_call_hook(unit, unit->on_configure);
    unit->time.configure = sp_tm_read_timer(&timer);
  }

  return result;
}

spn_err_t spn_pkg_unit_run_package_hook(spn_pkg_unit_t* ctx) {
  spn_err_t result = SPN_OK;

  if (ctx->on_package) {
    sp_tm_timer_t timer = sp_tm_start_timer();
    result = spn_pkg_unit_call_hook(ctx, ctx->on_package);
    ctx->time.package = sp_tm_read_timer(&timer);
  }

  return result;
}


s32 spn_executor_sync_repo(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* build = (spn_pkg_unit_t*)user_data;

  spn_event_buffer_push_ctx(spn.events, &build->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC,
    .sync = {
      .url = spn_pkg_get_url(build->ctx.pkg)
    }
  });
  spn_pkg_unit_sync_remote(build);

  sp_str_t message = spn_git_get_commit_message(build->ctx.paths.source, build->metadata.commit);
  message = sp_str_truncate(message, 32, SP_LIT("..."));
  message = sp_str_replace_c8(message, '\n', ' ');
  message = sp_str_replace_c8(message, '{', '['); // @spader @hack
  message = sp_str_replace_c8(message, '}', ']');
  message = sp_str_pad(message, 32);

  spn_event_buffer_push_ctx(spn.events, &build->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_CHECKOUT,
    .checkout = {
      .commit = spn_intern(build->metadata.commit),
      .version = build->metadata.version,
      .message = spn_intern(message)
    }
  });
  spn_pkg_unit_sync_local(build);

  return SPN_OK;
}

s32 spn_executor_build_target_unit_2(spn_bg_cmd_t* cmd, void* user_data) {
  // @target run the compiler, log and emit as needed
  return SPN_OK;
}

spn_cc_t* make_cc_for_compile_or_link(spn_pkg_t* pkg, spn_target_t* target, sp_str_t path, spn_profile_t* profile) {
  spn_cc_t* cc = sp_alloc_type(spn_cc_t);
  spn_cc_set_profile(cc, profile);
  spn_cc_set_output_dir(cc, path);

  sp_da_for(pkg->include, it) {
    spn_cc_add_include(cc, pkg->include[it]);
  }
  sp_da_for(pkg->define, it) {
    spn_cc_add_define(cc, pkg->define[it]);
  }
  return cc;
}

void setup_target_for_compile_or_link(spn_cc_t* cc, spn_cc_target_t* cc_target, spn_target_t* target, spn_pkg_t* pkg, spn_session_t* session) {
  sp_da_for(target->include, i) {
    spn_cc_target_add_relative_include(cc_target, target->include[i]);
  }

  sp_da_for(target->define, i) {
    spn_cc_target_add_define(cc_target, target->define[i]);
  }

  sp_da_for(pkg->system_deps, i) {
    spn_cc_target_add_lib(cc_target, spn_gen_format_entry(pkg->system_deps[i], SPN_GEN_SYSTEM_LIBS, cc->compiler.kind));
  }

  sp_ht_for_kv(pkg->deps, i) {
    if (spn_is_visibility_linked(target->visibility, i.val->visibility)) {
      spn_pkg_unit_t* dep = spn_session_find_pkg(session, *i.key);
      spn_cc_target_add_dep(cc_target, dep);

      sp_da_for(dep->ctx.pkg->system_deps, n) {
        spn_cc_target_add_lib(cc_target, spn_gen_format_entry(dep->ctx.pkg->system_deps[n], SPN_GEN_SYSTEM_LIBS, cc->compiler.kind));
      }
    }
  }
}

spn_err_t run_cc(spn_cc_t* cc, spn_cc_target_t* cc_target, sp_str_t cwd, spn_pkg_t* pkg, spn_build_io_t* io) {
  sp_ps_config_t ps = {
    .cwd = cwd,
    .io = {
      .in.mode  = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    }
  };
  spn_cc_to_ps(cc, &ps);
  spn_cc_target_to_ps(cc, cc_target, &ps);

  sp_mem_scratch_t scratch = sp_mem_begin_scratch(); {
    sp_str_builder_t log = SP_ZERO_INITIALIZE();
    sp_str_builder_append(&log, cc->compiler.exe);
    sp_str_builder_append_c8(&log, ' ');

    sp_da_for(ps.dyn_args, it) {
      sp_str_builder_append(&log, ps.dyn_args[it]);
      sp_str_builder_append_c8(&log, ' ');
    }

    spn_push_event_ex((spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD,
      .target.build = {
        .args = sp_str_builder_to_str(&log),
      }
    });

    sp_mem_end_scratch(scratch);
  }


  sp_ps_output_t result = sp_ps_run(ps);
  if (result.status.exit_code) {
    spn_event_buffer_push_ex(spn.events, pkg, io, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_FAILED,
      .target.failed = {
        .out = result.out,
        .err = result.err,
      }
    });

    return SPN_ERROR;
  } else {
    spn_event_buffer_push_ex(spn.events, pkg, io, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_PASSED
    });
  }

  return SPN_OK;
}

s32 spn_executor_compile_object(spn_bg_cmd_t* cmd, void* user_data) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;

  //spn_log_error("compile: {}", SP_FMT_STR(unit->paths.object));

  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->target->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_DEBUG,
    .debug = {
      .message = unit->paths.object
    }
  } );

  sp_str_t file = sp_fs_get_name(unit->paths.object);

  spn_cc_t* cc = make_cc_for_compile_or_link(unit->pkg, unit->target->info, unit->target->paths.object, unit->profile);
  spn_cc_target_t* cc_target = spn_cc_add_target(cc, SPN_TARGET_OBJECT, file);
  setup_target_for_compile_or_link(cc, cc_target, unit->target->info, unit->pkg, unit->session);

  spn_cc_target_add_absolute_source(cc_target, unit->paths.source);

  return run_cc(cc, cc_target, unit->target->paths.work, unit->pkg, &unit->target->logs);
}

s32 spn_executor_link_target(spn_bg_cmd_t* cmd, void* user_data) {
  spn_target_unit_t* unit = (spn_target_unit_t*)user_data;
  spn_target_t* target = unit->info;

  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_TARGET,
    .target_link = {
      .target = target->name,
      .objects = sp_da_size(unit->objects),
    }
  });

  //spn_log_error("link: {}", SP_FMT_STR(unit->target->name));

  spn_cc_t* cc = make_cc_for_compile_or_link(unit->pkg, unit->info, unit->paths.bin, unit->session->profile);
  spn_cc_target_t* cc_target = spn_cc_add_target(cc, SPN_TARGET_EXE, target->name);
  setup_target_for_compile_or_link(cc, cc_target, target, unit->pkg, unit->session);

  sp_da_for(unit->objects, it) {
    spn_cc_target_add_absolute_source(cc_target, unit->objects[it]->paths.object);
  }

  return run_cc(cc, cc_target, unit->paths.work, unit->pkg, &unit->logs);


  if (!sp_da_empty(target->embed)) {
    // spn_cc_embed_ctx_t embedder = SP_ZERO_INITIALIZE();
    // spn_cc_embed_ctx_init(&embedder);
    //
    // sp_da_for(target->embed, it) {
    //   spn_embed_t embed = target->embed[it];
    //   sp_str_t symbol = embed.symbol;
    //   spn_embed_types_t types = embed.types;
    //   sp_io_reader_t io = SP_ZERO_INITIALIZE();
    //
    //   if (sp_str_empty(embed.types.size)) {
    //     embed.types.data = spn_intern_cstr("unsigned char");
    //     embed.types.size = spn_intern_cstr("unsigned long long");
    //   }
    //
    //   switch (embed.kind) {
    //     case SPN_EMBED_MEM: {
    //       io = sp_io_reader_from_mem(embed.memory.buffer, embed.memory.size);
    //       break;
    //     }
    //     case SPN_EMBED_FILE: {
    //       if (!sp_fs_exists(embed.file.path)) {
    //         return SPN_ERROR;
    //       }
    //
    //       io = sp_io_reader_from_file(embed.file.path);
    //
    //       if (sp_str_empty(symbol)) {
    //         symbol = spn_cc_symbol_from_embedded_file(embed.file.path);
    //       }
    //       break;
    //     }
    //   }
    //
    //   spn_cc_embed_ctx_add(&embedder, io, symbol, embed.types.data, embed.types.size);
    // }
    //
    // sp_str_t object = sp_fs_join_path(unit->paths.generated, sp_format("{}.embed.o", SP_FMT_STR(unit->pkg->name)));
    // sp_str_t header = sp_fs_join_path(unit->paths.generated, sp_format("{}.embed.h", SP_FMT_STR(unit->pkg->name)));
    // spn_cc_embed_ctx_write(&embedder, object, header);
    // spn_cc_target_add_lib(cc_target, object);
    // spn_cc_target_add_include_abs(cc_target, unit->paths.generated);
  }

    // sp_da_for(app.resolver.system_deps, i) {
    //   sp_str_t arg = spn_gen_format_entry(app.resolver.system_deps[i], SPN_GEN_SYSTEM_LIBS, ctx->profile->cc.kind);
    //   sp_ps_config_add_arg(&ps, arg);
    // }





  // sp_mem_scratch_t scratch = sp_mem_begin_scratch(); {
  //   sp_str_builder_t log = SP_ZERO_INITIALIZE();
  //   sp_str_builder_append(&log, ctx->profile->cc.exe);
  //   sp_str_builder_append_c8(&log, ' ');
  //
  //   sp_da_for(ps.dyn_args, it) {
  //     sp_str_builder_append(&log, ps.dyn_args[it]);
  //     sp_str_builder_append_c8(&log, ' ');
  //   }
  //
  //   spn_push_event_ex((spn_build_event_t) {
  //     .kind = SPN_EVENT_TARGET_BUILD,
  //     .target.build = {
  //       .args = sp_str_builder_to_str(&log),
  //     }
  //   });
  //
  //   sp_mem_end_scratch(scratch);
  // }
  //
  // sp_ps_output_t result = spn_build_ctx_subprocess(ctx, ps);
  // if (result.status.exit_code) {
  //   spn_event_buffer_push_ctx(spn.events, ctx, (spn_build_event_t) {
  //     .kind = SPN_EVENT_TARGET_BUILD_FAILED,
  //     .target.failed = {
  //       .out = result.out,
  //       .err = result.err,
  //     }
  //   });
  //
  //   return SPN_ERROR;
  // } else {
  //   spn_event_buffer_push(spn.events, ctx, SPN_EVENT_TARGET_BUILD_PASSED);
  // }

  return SPN_OK;
}

sp_str_t spn_compiler_to_str(spn_cc_kind_t compiler) {
  return sp_str_lit("clang");
}

void spn_build_event_init(spn_build_event_t* event, spn_build_event_kind_t kind, spn_build_ctx_t* ctx) {
  event->kind = kind;
  event->pkg = ctx->pkg;
  event->io = &ctx->logs;
}

spn_build_event_t spn_build_event_make(spn_build_ctx_t* ctx, spn_build_event_kind_t kind) {
  spn_build_event_t event = SP_ZERO_INITIALIZE();
  spn_build_event_init(&event, kind, ctx);
  return event;
}


spn_event_buffer_t* spn_event_buffer_new() {
  spn_event_buffer_t* events = SP_ALLOC(spn_event_buffer_t);
  return events;
}

void spn_event_buffer_push(spn_event_buffer_t* events, spn_build_ctx_t* ctx, spn_build_event_kind_t kind) {
  spn_event_buffer_push_ctx(events, ctx, spn_build_event_make(ctx, kind));
}

void spn_event_buffer_push_ctx(spn_event_buffer_t* events, spn_build_ctx_t* ctx, spn_build_event_t config) {
  spn_build_event_t event = config;
  spn_build_event_init(&event, event.kind, ctx);

  sp_mutex_lock(&events->mutex);
  sp_rb_push(events->buffer, event);
  sp_mutex_unlock(&events->mutex);
}

void spn_event_buffer_push_ex(spn_event_buffer_t* events, spn_pkg_t* pkg, spn_build_io_t* io, spn_build_event_t e) {
  spn_build_event_t event = e;
  event.pkg = pkg;
  event.io = io;

  sp_mutex_lock(&events->mutex);
  sp_rb_push(events->buffer, event);
  sp_mutex_unlock(&events->mutex);
}

sp_da(spn_build_event_t) spn_event_buffer_drain(spn_event_buffer_t* events) {
  sp_mutex_lock(&events->mutex);

  sp_da(spn_build_event_t) result = SP_NULLPTR;
  sp_rb_for(events->buffer, it) {
    spn_build_event_t* event = &sp_rb_at(events->buffer, it);
    sp_da_push(result, *event);
  }

  sp_rb_clear(events->buffer);
  sp_mutex_unlock(&events->mutex);

  return result;
}

/////////
// APP //
/////////
void spn_session_init(spn_session_t* session, spn_pkg_t* pkg, spn_profile_t* profile, sp_str_t dir) {
  session->pkg = pkg;
  session->profile = profile;
  session->paths.root = sp_str_copy(pkg->paths.root);
  session->paths.build = sp_fs_join_path(session->paths.root, dir);
  session->paths.profile = sp_fs_join_path(session->paths.build, session->profile->name);


  sp_mutex_init(&session->mutex, SP_MUTEX_PLAIN);
}

spn_pkg_unit_t* spn_session_find_pkg(spn_session_t* session, sp_str_t name) {
  sp_mutex_lock(&session->mutex);
  spn_pkg_unit_t* pkg = sp_str_equal(name, session->pkg->name) ?
    &session->units.root :
    sp_om_get(session->units.packages, name);
  sp_mutex_unlock(&session->mutex);

  return pkg;
}

spn_pkg_unit_t* spn_session_find_root(spn_session_t* s) {
  return spn_session_find_pkg(s, s->pkg->name);
}

void spn_session_set_filter(spn_session_t* session, spn_target_filter_t filter) {
  session->filter = filter;
}

void spn_pkg_unit_add_target(spn_pkg_unit_t* pkg, spn_target_t* target) {
  spn_session_t* session = pkg->ctx.session;

  spn_event_buffer_push_ex(spn.events, pkg->ctx.pkg, &pkg->ctx.logs, (spn_build_event_t) {
    .kind = SPN_EVENT_ADD_TARGET,
    .target_add = {
      .target = target->name,
      .kind = target->kind,
    }
  });

  sp_om_insert(pkg->targets, target->name, SP_ZERO_STRUCT(spn_target_unit_t));
  spn_target_unit_t* unit = sp_om_back(pkg->targets);
  unit->session = pkg->ctx.session;
  unit->pkg = pkg->ctx.pkg;
  unit->info = target;

  spn_bp_config_t config = {
    .source = pkg->ctx.paths.source,
    .store = pkg->ctx.paths.store,
    .work = pkg->ctx.paths.work,
  };

  unit->paths.source = sp_str_copy(config.source);
  unit->paths.work = sp_str_copy(config.work);
  unit->paths.store = sp_str_copy(config.store);
  unit->paths.include = sp_fs_join_path(unit->paths.store, SP_LIT("include"));
  unit->paths.bin = sp_fs_join_path(unit->paths.store, SP_LIT("bin"));
  unit->paths.lib = sp_fs_join_path(unit->paths.store, SP_LIT("lib"));
  unit->paths.vendor = sp_fs_join_path(unit->paths.store, SP_LIT("vendor"));
  unit->paths.generated = sp_fs_join_path(unit->paths.work, SP_LIT("spn"));
  unit->paths.object = sp_fs_join_path(unit->paths.generated, sp_str_lit("object"));
  unit->paths.logs.build = sp_fs_join_path(unit->paths.work, sp_format("{}.build.log", SP_FMT_STR(target->name)));
  unit->paths.logs.test = sp_fs_join_path(unit->paths.work, sp_format("{}.test.log", SP_FMT_STR(target->name)));

  sp_fs_create_dir(unit->paths.work);
  sp_fs_create_dir(unit->paths.generated);
  sp_fs_create_dir(unit->paths.object);
  sp_fs_create_dir(unit->paths.store);
  sp_fs_create_dir(unit->paths.bin);
  sp_fs_create_dir(unit->paths.include);
  sp_fs_create_dir(unit->paths.lib);
  sp_fs_create_dir(unit->paths.vendor);
  sp_fs_create_file(unit->paths.logs.build);

  unit->logs.build = sp_io_writer_from_file(unit->paths.logs.build, SP_IO_WRITE_MODE_APPEND);

  spn_event_buffer_push_ex(spn.events, pkg->ctx.pkg, &pkg->ctx.logs, (spn_build_event_t) {
    .kind = SPN_EVENT_ADD_TARGET,
    .target.add = {
      .name = target->name
    }
  });

}

void spn_init_pkg_unit_for_session(spn_session_t* session, spn_pkg_unit_t* unit, spn_pkg_t* pkg, spn_pkg_kind_t kind, spn_semver_t version) {
  switch (kind) {
    case SPN_PACKAGE_KIND_ROOT:
    case SPN_PACKAGE_KIND_FILE: {
      spn_pkg_unit_init(unit, (spn_pkg_unit_config_t)  {
        .ctx = {
          .name = pkg->name,
          .package = pkg,
          .session = session,
          .paths = {
            .store = sp_fs_join_path(session->paths.profile, sp_str_lit("store")),
            .work = sp_fs_join_path(session->paths.profile, sp_format("work/{}", SP_FMT_STR(pkg->name))),
            .source = sp_str_copy(pkg->paths.root),
          }
        }
      });
      break;
    }
    case SPN_PACKAGE_KIND_INDEX: {
      sp_opt(spn_linkage_t) linkage = SP_ZERO_INITIALIZE();

      spn_dep_options_t* options = sp_ht_getp(pkg->config, pkg->name);
      if (options) {
        spn_dep_option_t* kind = sp_ht_getp(*options, sp_str_lit("kind"));
        if (kind) {
          sp_opt_set(linkage, spn_lib_kind_from_str(kind->str));
        }
      }

      switch (linkage.some) {
        case SP_OPT_SOME: {
          break;
        }
        case SP_OPT_NONE: {
          if (spn_pkg_has_lib_kind(pkg, SPN_LIB_KIND_SOURCE)) { sp_opt_set(linkage, SPN_LIB_KIND_SOURCE); break; }
          if (spn_pkg_has_lib_kind(pkg, SPN_LIB_KIND_STATIC)) { sp_opt_set(linkage, SPN_LIB_KIND_STATIC); break; }
          if (spn_pkg_has_lib_kind(pkg, SPN_LIB_KIND_SHARED)) { sp_opt_set(linkage, SPN_LIB_KIND_SHARED); break; }
          sp_unreachable();
        }
      }

      spn_pkg_metadata_t* metadata = sp_ht_getp(pkg->metadata, version);
      sp_assert(metadata);

      sp_dyn_array(sp_hash_t) hashes = SP_NULLPTR;
      sp_dyn_array_push(hashes, sp_hash_str(metadata->commit));
      sp_dyn_array_push(hashes, sp_hash_str(session->profile->cc.exe));
      sp_dyn_array_push(hashes, session->profile->cc.kind);
      sp_dyn_array_push(hashes, session->profile->libc);
      sp_dyn_array_push(hashes, session->profile->mode);
      sp_dyn_array_push(hashes, linkage.value);
      sp_dyn_array_push(hashes, metadata->version.major);
      sp_dyn_array_push(hashes, metadata->version.minor);
      sp_dyn_array_push(hashes, metadata->version.patch);
      sp_hash_t build_id = sp_hash_combine(hashes, sp_dyn_array_size(hashes));
      sp_str_t build_str = sp_format("{}", SP_FMT_SHORT_HASH(build_id));

      spn_pkg_unit_init(unit, (spn_pkg_unit_config_t)  {
        .build_id = build_id,
        .metadata = *metadata,
        .ctx = {
          .name = pkg->name,
          .package = pkg,
          .session = session,
          .linkage = linkage.value,
          .paths = {
            .work = sp_fs_join_path(pkg->paths.cache.work, build_str),
            .store = sp_fs_join_path(pkg->paths.cache.store, build_str),
            .source = pkg->paths.cache.source
          }
        }
      });
      break;
    }
    case SPN_PACKAGE_KIND_WORKSPACE:
    case SPN_PACKAGE_KIND_NONE: {
      SP_UNREACHABLE_CASE();
    }
  }

  switch (kind) {
    case SPN_PACKAGE_KIND_ROOT:
    case SPN_PACKAGE_KIND_FILE:
    case SPN_PACKAGE_KIND_INDEX: {
      sp_om_for(pkg->exes, it) {
        spn_target_t* target = sp_om_at(pkg->exes, it);
        if (spn_target_filter_pass(&session->filter, target)) {
          spn_pkg_unit_add_target(unit, target);
        }
      }

      sp_om_for(pkg->libs, it) {
        spn_target_t* target = sp_om_at(pkg->libs, it);
        if (spn_target_filter_pass(&session->filter, target)) {
          spn_pkg_unit_add_target(unit, target);
        }
      }

      break;
    }
    case SPN_PACKAGE_KIND_WORKSPACE:
    case SPN_PACKAGE_KIND_NONE: {
      sp_unreachable_case();
    }
  }

  switch (kind) {
    case SPN_PACKAGE_KIND_ROOT:
    case SPN_PACKAGE_KIND_FILE: {
      sp_om_for(pkg->tests, it) {
        spn_target_t* target = sp_om_at(pkg->tests, it);
        if (spn_target_filter_pass(&session->filter, target)) {
          spn_pkg_unit_add_target(unit, target);
        }
      }

      break;
    }
    case SPN_PACKAGE_KIND_INDEX: {
      break;
    }
    case SPN_PACKAGE_KIND_WORKSPACE:
    case SPN_PACKAGE_KIND_NONE: {
      sp_unreachable_case();
    }
  }

}

void spn_session_add_pkg_unit(spn_session_t* session, spn_resolved_pkg_t resolved) {
  sp_om_insert(session->units.packages, resolved.pkg->name, SP_ZERO_STRUCT(spn_pkg_unit_t));
  spn_pkg_unit_t* unit = sp_om_back(session->units.packages);
  spn_init_pkg_unit_for_session(session, unit, resolved.pkg, resolved.kind, resolved.version);
}

void spn_app_update_lock_file(spn_app_t* app) {
  spn_lock_file_t lock = spn_build_lock_file();

  // Add top-level package's system_deps to lock
  sp_da_for(app->package.system_deps, i) {
    sp_ht_insert(lock.system_deps, sp_str_copy(app->package.system_deps[i]), true);
  }

  sp_da(sp_str_t) keys = SP_NULLPTR;
  sp_ht_collect_keys(lock.entries, keys);
  sp_dyn_array_sort(keys, sp_str_sort_kernel_alphabetical);

  spn_toml_writer_t toml = spn_toml_writer_new();

  spn_toml_begin_table_cstr(&toml, "spn");
  spn_toml_append_str_cstr(&toml, "version", sp_str_lit(SPN_VERSION));
  spn_toml_append_str_cstr(&toml, "commit", sp_str_lit(SPN_COMMIT));
  spn_toml_end_table(&toml);

  // Write [package] table with system_deps
  if (sp_ht_size(lock.system_deps)) {
    spn_toml_begin_table_cstr(&toml, "package");
    sp_da(sp_str_t) sys_deps = SP_NULLPTR;
    sp_ht_collect_keys(lock.system_deps, sys_deps);
    sp_dyn_array_sort(sys_deps, sp_str_sort_kernel_alphabetical);
    spn_toml_append_str_array_cstr(&toml, "system_deps", sys_deps);
    spn_toml_end_table(&toml);
  }

  spn_toml_begin_array_cstr(&toml, "dep");
  sp_dyn_array_for(keys, it) {
    spn_lock_entry_t* entry = sp_ht_getp(lock.entries, keys[it]);

    spn_toml_append_array_table(&toml);
    spn_toml_append_str_cstr(&toml, "name", entry->name);
    spn_toml_append_str_cstr(&toml, "version", spn_semver_to_str(entry->version));
    spn_toml_append_str_cstr(&toml, "commit", entry->commit);
    spn_toml_append_str_cstr(&toml, "kind", spn_package_kind_to_str(entry->kind));
    spn_toml_append_str_cstr(&toml, "visibility", spn_visibility_to_str(entry->visibility));

    if (sp_dyn_array_size(entry->deps)) {
      spn_toml_append_str_array_cstr(&toml, "deps", entry->deps);
    }
  }
  spn_toml_end_array(&toml);

  sp_str_t output = spn_toml_writer_write(&toml);
  sp_io_writer_t file = sp_io_writer_from_file(app->paths.lock, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_str(&file, output);
  sp_io_writer_close(&file);
}

void spn_app_write_manifest(spn_pkg_t* pkg, sp_str_t path) {
  spn_toml_writer_t toml = spn_toml_writer_new();

  spn_toml_begin_table_cstr(&toml, "package");
  spn_toml_append_str_cstr(&toml, "name", pkg->name);
  spn_toml_append_str_cstr(&toml, "version", spn_semver_to_str(pkg->version));
  if (!sp_str_empty(pkg->url)) {
    spn_toml_append_str_cstr(&toml, "url", pkg->url);
  }
  if (!sp_str_empty(pkg->author)) {
    spn_toml_append_str_cstr(&toml, "author", pkg->author);
  }
  if (!sp_str_empty(pkg->maintainer)) {
    spn_toml_append_str_cstr(&toml, "maintainer", pkg->maintainer);
  }
  if (!sp_dyn_array_empty(pkg->include)) {
    spn_toml_append_str_array_cstr(&toml, "include", pkg->include);
  }
  if (!sp_dyn_array_empty(pkg->system_deps)) {
    spn_toml_append_str_array_cstr(&toml, "system_deps", pkg->system_deps);
  }
  if (!sp_dyn_array_empty(pkg->define)) {
    spn_toml_append_str_array_cstr(&toml, "define", pkg->define);
  }
  spn_toml_end_table(&toml);

  if (sp_ht_size(pkg->deps)) {
    // Write package deps
    bool has_package_deps = false;
    sp_ht_for_kv(pkg->deps, it) {
      if (it.val->visibility == SPN_VISIBILITY_PUBLIC) {
        has_package_deps = true;
        break;
      }
    }
    if (has_package_deps) {
      spn_toml_begin_table_cstr(&toml, "deps.package");
      sp_ht_for_kv(pkg->deps, it) {
        if (it.val->visibility == SPN_VISIBILITY_PUBLIC) {
          spn_toml_append_str(&toml, *it.key, spn_pkg_req_to_str(*it.val));
        }
      }
      spn_toml_end_table(&toml);
    }

    // Write build deps
    bool has_build_deps = false;
    sp_ht_for_kv(pkg->deps, it) {
      if (it.val->visibility == SPN_VISIBILITY_BUILD) {
        has_build_deps = true;
        break;
      }
    }
    if (has_build_deps) {
      spn_toml_begin_table_cstr(&toml, "deps.build");
      sp_ht_for_kv(pkg->deps, it) {
        if (it.val->visibility == SPN_VISIBILITY_BUILD) {
          spn_toml_append_str(&toml, *it.key, spn_pkg_req_to_str(*it.val));
        }
      }
      spn_toml_end_table(&toml);
    }

    // Write test deps
    bool has_test_deps = false;
    sp_ht_for_kv(pkg->deps, it) {
      if (it.val->visibility == SPN_VISIBILITY_TEST) {
        has_test_deps = true;
        break;
      }
    }
    if (has_test_deps) {
      spn_toml_begin_table_cstr(&toml, "deps.test");
      sp_ht_for_kv(pkg->deps, it) {
        if (it.val->visibility == SPN_VISIBILITY_TEST) {
          spn_toml_append_str(&toml, *it.key, spn_pkg_req_to_str(*it.val));
        }
      }
      spn_toml_end_table(&toml);
    }
  }

  if (!sp_om_empty(pkg->profiles)) {
    spn_toml_begin_array_cstr(&toml, "profile");
    sp_om_for(pkg->profiles, it) {
      spn_profile_t* profile = sp_om_at(pkg->profiles, it);
      if (profile->kind != SPN_PROFILE_BUILTIN) {
        spn_toml_append_array_table(&toml);
        spn_toml_append_str_cstr(&toml, "name", profile->name);
        spn_toml_append_str_cstr(&toml, "cc", profile->cc.exe);
        spn_toml_append_str_cstr(&toml, "linkage", spn_pkg_linkage_to_str(profile->linkage));
        spn_toml_append_str_cstr(&toml, "libc", spn_libc_kind_to_str(profile->libc));
        spn_toml_append_str_cstr(&toml, "standard", spn_c_standard_to_str(profile->standard));
        spn_toml_append_str_cstr(&toml, "mode", spn_dep_build_mode_to_str(profile->mode));
      }
    }
    spn_toml_end_array(&toml);
  }

  if (!sp_om_empty(pkg->libs)) {
    spn_toml_begin_array_cstr(&toml, "lib");
    sp_om_for(pkg->libs, it) {
      spn_target_t* lib = sp_om_at(pkg->libs, it);
      spn_toml_append_array_table(&toml);
      spn_toml_append_str_cstr(&toml, "name", lib->name);

      spn_linkage_t linkage = spn_target_kind_to_pkg_linkage(lib->kind);
      spn_toml_append_str_cstr(&toml, "kind", spn_pkg_linkage_to_str(linkage));

      if (sp_dyn_array_size(lib->source)) {
        spn_toml_append_str_array_cstr(&toml, "source", lib->source);
      }
      if (sp_dyn_array_size(lib->include)) {
        spn_toml_append_str_array_cstr(&toml, "include", lib->include);
      }
      if (sp_dyn_array_size(lib->define)) {
        spn_toml_append_str_array_cstr(&toml, "define", lib->define);
      }
    }
    spn_toml_end_array(&toml);
  }

  if (!sp_om_empty(pkg->exes)) {
    spn_toml_begin_array_cstr(&toml, "bin");
    sp_om_for(pkg->exes, it) {
      spn_target_t* bin = sp_om_at(pkg->exes, it);
      spn_toml_append_array_table(&toml);
      spn_toml_append_str_cstr(&toml, "name", bin->name);

      if (bin->visibility != SPN_VISIBILITY_PUBLIC) {
        spn_toml_append_str_cstr(&toml, "kind", spn_visibility_to_str(bin->visibility));
      }
      if (sp_dyn_array_size(bin->source)) {
        spn_toml_append_str_array_cstr(&toml, "source", bin->source);
      }
      if (sp_dyn_array_size(bin->include)) {
        spn_toml_append_str_array_cstr(&toml, "include", bin->include);
      }
      if (sp_dyn_array_size(bin->define)) {
        spn_toml_append_str_array_cstr(&toml, "define", bin->define);
      }
    }
    spn_toml_end_array(&toml);
  }

  if (!sp_om_empty(pkg->tests)) {
    spn_toml_begin_array_cstr(&toml, "test");
    sp_om_for(pkg->tests, it) {
      spn_target_t* test = sp_om_at(pkg->tests, it);

      spn_toml_append_array_table(&toml);
      spn_toml_append_str_cstr(&toml, "name", test->name);

      if (sp_dyn_array_size(test->source)) {
        spn_toml_append_str_array_cstr(&toml, "source", test->source);
      }
      if (sp_dyn_array_size(test->include)) {
        spn_toml_append_str_array_cstr(&toml, "include", test->include);
      }
      if (sp_dyn_array_size(test->define)) {
        spn_toml_append_str_array_cstr(&toml, "define", test->define);
      }
    }
    spn_toml_end_array(&toml);
  }

  if (sp_ht_size(pkg->options)) {
    spn_toml_begin_table_cstr(&toml, "options");
    sp_ht_for_kv(pkg->options, it) {
      spn_toml_append_option(&toml, *it.key, *it.val);
    }
    spn_toml_end_table(&toml);
  }

  if (sp_ht_size(pkg->config)) {
    spn_toml_begin_table_cstr(&toml, "config");

    sp_ht_for_kv(pkg->config, it) {
      spn_toml_begin_table(&toml, *it.key);
      sp_ht_for_kv(*it.val, n) {
        spn_toml_append_option(&toml, *n.key, *n.val);
      }
      spn_toml_end_table(&toml);
    }

    spn_toml_end_table(&toml);
  }

  if (!sp_om_empty(pkg->registries)) {
    spn_toml_begin_array_cstr(&toml, "registry");
    sp_om_for(pkg->registries, it) {
      spn_registry_t* registry = sp_om_at(pkg->registries, it);

      spn_toml_append_array_table(&toml);

      spn_toml_append_str_cstr(&toml, "name", registry->name);
      spn_toml_append_str_cstr(&toml, "location", registry->location);
    }
    spn_toml_end_array(&toml);
  }

  sp_str_t output = spn_toml_writer_write(&toml);
  output = sp_str_trim_right(output);
  sp_io_writer_t file = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_str(&file, output);
  sp_io_writer_close(&file);
}

spn_pkg_t* spn_app_find_package(spn_app_t* app, sp_str_t name) {
  return sp_om_get(app->cache, name);
}

spn_pkg_t* spn_app_find_package_from_request(spn_app_t* app, spn_pkg_req_t request) {
  spn_pkg_t* package = spn_app_find_package(app, request.name);
  if (package->kind != request.kind) {
    return SP_NULLPTR;
  }

  return package;
}

spn_pkg_t* spn_app_ensure_package(spn_app_t* app, spn_pkg_req_t request) {
  sp_str_t name = spn_intern(request.name);

  if (!sp_om_has(app->cache, name)) {
    sp_om_insert(app->cache, name, SP_ZERO_STRUCT(spn_pkg_t));
    spn_pkg_t* pkg = sp_om_get(app->cache, name);
    spn_pkg_init(pkg, name);

    switch (request.kind) {
      case SPN_PACKAGE_KIND_FILE: {
        sp_str_t prefix = sp_str_lit("file://");
        sp_str_t manifest = {
          .data = request.file.data + prefix.len,
          .len = request.file.len - prefix.len
        };
        spn_pkg_from_manifest(pkg, manifest);

        break;
      }
      case SPN_PACKAGE_KIND_INDEX: {
        sp_str_t* path = sp_ht_getp(app->registry, name);
        if (!path) {
          spn_app_bail_on_missing_package(app, name);
        }

        spn_pkg_from_index(pkg, *path);

        break;
      }
      case SPN_PACKAGE_KIND_ROOT:
      case SPN_PACKAGE_KIND_WORKSPACE: {
        SP_FATAL("unimplemented find_package");
        break;
      }
      case SPN_PACKAGE_KIND_NONE: {
        SP_UNREACHABLE_RETURN(SP_NULLPTR);
      }
    }
  }

  return spn_app_find_package_from_request(app, request);
}

spn_app_t spn_app_new() {
  spn_app_t app = SP_ZERO_INITIALIZE();

  sp_ht_set_fns(app.registry, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);

  return app;
}

spn_app_t spn_app_init_and_write(sp_str_t path, sp_str_t name, spn_app_init_mode_t mode) {
  sp_str_t paths [] = {
    sp_fs_join_path(path, sp_str_lit("spn.toml")),
    sp_fs_join_path(path, sp_str_lit("spn.c")),
  };
  sp_carr_for(paths, it) {
    if (sp_fs_exists(paths[it])) {
      SP_FATAL("{:fg brightcyan} already exists; bailing", SP_FMT_STR(paths[it]));
    }
  }

  spn_app_t app = spn_app_new();
  switch (mode) {
    case SPN_APP_INIT_NORMAL: {
      app.package = spn_pkg_from_default(path, name);

      sp_str_t main = sp_fs_join_path(path, sp_str_lit("main.c"));
      sp_io_writer_t io = sp_io_writer_from_file(main, SP_IO_WRITE_MODE_OVERWRITE);

      sp_str_t content = sp_str_lit(
        "#define SP_IMPLEMENTATION\n"
        "#include \"sp.h\"\n"
        "\n"
        "s32 main(s32 num_args, const c8** args) {\n"
        "  SP_LOG(\"hello, {:fg brightcyan}\", SP_FMT_CSTR(\"world\"));\n"
        "  SP_EXIT_SUCCESS();\n"
        "}\n"
      );

      if (sp_io_write_str(&io, content) != content.len) {
        SP_FATAL("Failed to write {:fg brightyellow}", SP_FMT_STR(main));
      }

      sp_io_writer_close(&io);

      spn_app_write_manifest(&app.package, app.package.paths.manifest);

      break;
    }
    case SPN_APP_INIT_BARE: {
      app.package = spn_pkg_new(name);
      spn_pkg_set_manifest(&app.package, sp_fs_join_path(path, SP_LIT("spn.toml")));
      spn_app_write_manifest(&app.package, app.package.paths.manifest);

      break;
    }
  }

  return app;
}

void spn_app_load(spn_app_t* app, sp_str_t manifest_path) {
  // Load the top level package
  if (sp_fs_exists(manifest_path)) {
    spn_pkg_from_manifest(&app->package, manifest_path);
  }

  app->paths.dir = app->package.paths.root;
  app->paths.lock = sp_fs_join_path(app->paths.dir, SP_LIT("spn.lock"));

  // Now that we know all the registries, discover all packages
  sp_dyn_array_push(app->search, spn_registry_get_path(&spn.registry));

  sp_dyn_array_for(spn.config.registries, it) {
    spn_registry_t* registry = &spn.config.registries[it];
    sp_dyn_array_push(app->search, spn_registry_get_path(registry));
  }

  sp_om_for(app->package.registries, it) {
    spn_registry_t* registry = sp_om_at(app->package.registries, it);
    sp_dyn_array_push(app->search, spn_registry_get_path(registry));
  }

  sp_dyn_array_for(app->search, i) {
    sp_str_t path = app->search[i];
    if (!sp_fs_exists(path)) continue;
    if (!sp_fs_is_dir(path)) {
      SP_FATAL(
        "{:fg brightcyan} is on the search path, but it's not a directory",
        SP_FMT_STR(path)
      );
    }

    sp_da(sp_os_dir_ent_t) entries = sp_fs_collect(path);
    sp_dyn_array_for(entries, i) {
      sp_os_dir_ent_t entry = entries[i];
      sp_str_t stem = sp_fs_get_stem(entry.file_path);
      sp_ht_insert(app->registry, stem, entry.file_path);
    }
  }

  // Load the lock file
  if (sp_fs_exists(app->paths.lock)) {
    sp_opt_set(app->lock, spn_lock_file_load(app->paths.lock));
  }

  // apply any defaults
  if (sp_om_empty(app->package.profiles)) {
    spn_profile_t profiles [] = {
      {
        .name = sp_str_lit("debug"),
        .linkage  = SPN_LIB_KIND_SHARED,
        .libc     = SPN_LIBC_GNU,
        .standard = SPN_C11,
        .mode     = SPN_DEP_BUILD_MODE_DEBUG,
        .kind     = SPN_PROFILE_BUILTIN,
        .cc = {
          .kind = SPN_CC_GCC,
          .exe = sp_str_lit("gcc")
        },
      },
      {
        .name     = sp_str_lit("release"),
        .linkage  = SPN_LIB_KIND_SHARED,
        .libc     = SPN_LIBC_GNU,
        .standard = SPN_C11,
        .mode     = SPN_DEP_BUILD_MODE_RELEASE,
        .kind     = SPN_PROFILE_BUILTIN,
        .cc = {
          .kind = SPN_CC_GCC,
          .exe = sp_str_lit("gcc")
        },
      }
    };
    sp_carr_for(profiles, it) {
      spn_pkg_add_profile_ex(&app->package, profiles[it]);
    }
  }
}

void spn_push_event(spn_build_event_kind_t kind) {
  spn_push_event_ex((spn_build_event_t) {
    .kind = kind
  });
}

void spn_push_event_ex(spn_build_event_t event) {
  spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(&app.session)->ctx, event);
}

#ifdef SP_POSIX
void spn_signal_handler(s32 kind) {
  switch (kind) {
    case SIGINT: {
      printf("sigint\n");
      sp_atomic_s32_set(&spn.sp->shutdown, 1);
      sp_io_write_new_line(&spn.logger.out);
      sp_io_write_new_line(&spn.logger.err);
      break;
    }
    default: {
      break;
    }
  }
}

void spn_install_signal_handlers() {
  struct sigaction sa;
  sa.sa_handler = spn_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
}
#else
sp_win32_bool_t spn_windows_console_handler(sp_win32_dword_t ctrl_type) {
  if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT) {
    sp_atomic_s32_set(&app->control, 1);
    printf("\n");
    fflush(stdout);
    return TRUE;
  }
  return FALSE;
}

void spn_install_signal_handlers() {
  SetConsoleCtrlHandler((PHANDLER_ROUTINE)spn_windows_console_handler, TRUE);
}
#endif



/////////
// APP //
/////////
sp_app_result_t spn_init(sp_app_t* sp) {
  spn.sp = sp;

  spn.intern = sp_intern_new();
  spn.arena = sp_mem_arena_new_ex(256, SP_MEM_ARENA_MODE_NO_REALLOC, 1);

  spn_install_signal_handlers();
  spn.logger.out = sp_io_writer_from_fd(STDOUT_FILENO, SP_IO_CLOSE_MODE_NONE);
  spn.logger.err = sp_io_writer_from_fd(STDERR_FILENO, SP_IO_CLOSE_MODE_NONE);

  spn.env = sp_alloc_type(sp_env_t);
  *spn.env = sp_env_capture();

  spn.log_level = SPN_LOG_LEVEL_INFO;
  sp_str_t log_level = sp_env_get(spn.env, sp_str_lit("SPN_LOG_LEVEL"));
  if (!sp_str_empty(log_level)) {
    spn.log_level = spn_log_level_from_str(log_level);
  }

  spn_tui_init(&spn.tui, SPN_OUTPUT_MODE_INTERACTIVE);

  spn.events = spn_event_buffer_new();

  sp_atomic_s32_set(&spn.control, 0);


  spn.paths.cwd = sp_fs_get_cwd();
  spn.paths.bin = sp_os_get_bin_path();

  sp_str_t storage = sp_env_get(spn.env, sp_str_lit("SPN_STORAGE_DIR"));
  if (sp_str_empty(storage)) {
    storage = sp_fs_join_path(sp_fs_get_storage_path(), sp_str_lit("spn"));
  }

  spn.paths.storage = storage;
  spn.paths.tools.dir = sp_fs_join_path(spn.paths.storage, sp_str_lit("tools"));
  spn.paths.tools.manifest = sp_fs_join_path(spn.paths.tools.dir, sp_str_lit("spn.toml"));
  spn.paths.tools.lock = sp_fs_join_path(spn.paths.storage, sp_str_lit("spn.lock"));

  // CONFIG
  sp_str_t config_dir = sp_env_get(spn.env, sp_str_lit("SPN_CONFIG_DIR"));
  if (sp_str_empty(config_dir)) {
    config_dir = sp_fs_get_config_path();
  }
  spn.paths.config_dir = sp_fs_join_path(config_dir, SP_LIT("spn"));
  spn.paths.config = sp_fs_join_path(spn.paths.config_dir, SP_LIT("spn.toml"));

  if (sp_fs_exists(spn.paths.config)) {
    toml_table_t* toml = spn_toml_parse(spn.paths.config);

    toml_value_t dir = toml_table_string(toml, "spn");
    if (dir.ok) {
      spn.paths.spn = sp_str_view(dir.u.s);
    }

    toml_array_t* registries = toml_table_array(toml, "registry");
    if (registries) {
      spn_toml_arr_for(registries, n) {
        toml_table_t* it = toml_array_table(registries, n);
        spn_registry_t registry = {
          .location = spn_toml_str(it, "location"),
          .kind = SPN_PACKAGE_KIND_INDEX
        };

        sp_dyn_array_push(spn.config.registries, registry);
      }
    }
  }

  if (!sp_str_valid(spn.paths.spn)) {
    spn.paths.spn = sp_fs_join_path(spn.paths.storage, sp_str_lit("spn"));
  }

  spn.paths.index = sp_fs_join_path(spn.paths.spn, sp_str_lit("packages"));
  spn.paths.include = sp_fs_join_path(spn.paths.spn, sp_str_lit("include"));

  if (!sp_fs_exists(spn.paths.spn)) {
    sp_str_t url = SP_LIT("https://github.com/tspader/spn.git");
    SP_LOG(
      "Cloning index from {:fg brightcyan} to {:fg brightcyan}",
      SP_FMT_STR(url),
      SP_FMT_STR(spn.paths.spn)
    );

    SP_ASSERT(!spn_git_clone(url, spn.paths.spn));
  }

  // Initialize builtin registry
  spn.registry = (spn_registry_t) {
    .location = spn.paths.index,
    .kind = SPN_PACKAGE_KIND_INDEX
  };

  // Find the cache directory after the config has been fully loaded
  spn.paths.runtime = sp_fs_join_path(spn.paths.storage, SP_LIT("runtime"));
  spn.paths.log = sp_fs_join_path(spn.paths.storage, SP_LIT("log"));
  spn.paths.cache = sp_fs_join_path(spn.paths.storage, SP_LIT("cache"));
  spn.paths.source = sp_fs_join_path(spn.paths.cache, SP_LIT("source"));
  spn.paths.build = sp_fs_join_path(spn.paths.cache, SP_LIT("build"));
  spn.paths.store = sp_fs_join_path(spn.paths.cache, SP_LIT("store"));

  sp_fs_create_dir(spn.paths.log);
  sp_fs_create_dir(spn.paths.cache);
  sp_fs_create_dir(spn.paths.source);
  sp_fs_create_dir(spn.paths.build);
  sp_fs_create_dir(spn.paths.store);
  sp_fs_create_dir(spn.paths.bin);
  sp_fs_create_dir(spn.paths.tools.dir);

  // @spader
  // spn_extract_runtime()
  if (!sp_fs_exists(spn.paths.runtime)) {
    sp_fs_create_dir(spn.paths.runtime);
    sp_fs_create_dir(sp_fs_join_path(spn.paths.runtime, sp_str_lit("include")));

    const struct { sp_str_t path; const u8* data; u64 size; } runtime [] = {
      { sp_str_lit("bcheck.o"), bcheck_o, bcheck_o_size },
      { sp_str_lit("bt-exe.o"), bt_exe_o, bt_exe_o_size },
      { sp_str_lit("bt-log.o"), bt_log_o, bt_log_o_size },
      { sp_str_lit("libtcc1.a"), libtcc1_a, libtcc1_a_size },
      { sp_str_lit("runmain.o"), runmain_o, runmain_o_size },
      { sp_str_lit("run_nostdlib.o"), run_nostdlib_o, run_nostdlib_o_size },
      { sp_str_lit("include/float.h"), include_float_h, include_float_h_size },
      { sp_str_lit("include/stdalign.h"), include_stdalign_h, include_stdalign_h_size },
      { sp_str_lit("include/stdarg.h"), include_stdarg_h, include_stdarg_h_size },
      { sp_str_lit("include/stdatomic.h"), include_stdatomic_h, include_stdatomic_h_size },
      { sp_str_lit("include/stdbool.h"), include_stdbool_h, include_stdbool_h_size },
      { sp_str_lit("include/stddef.h"), include_stddef_h, include_stddef_h_size },
      { sp_str_lit("include/stdnoreturn.h"), include_stdnoreturn_h, include_stdnoreturn_h_size },
      { sp_str_lit("include/tccdefs.h"), include_tccdefs_h, include_tccdefs_h_size },
      { sp_str_lit("include/tcclib.h"), include_tcclib_h, include_tcclib_h_size },
      { sp_str_lit("include/tgmath.h"), include_tgmath_h, include_tgmath_h_size },
      { sp_str_lit("include/varargs.h"), include_varargs_h, include_varargs_h_size },
    };
    sp_carr_for(runtime, it) {
      sp_str_t path = sp_fs_join_path(spn.paths.runtime, runtime[it].path);
      sp_io_writer_t io = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_OVERWRITE);
      sp_io_write(&io, runtime[it].data, runtime[it].size);
    }
  }

  spn_cli_t* cli = &spn.cli;
  spn.cli.cmd = spn_cli();

  spn_cli_parser_t parser = {
    .args = spn.args + 1,
    .num_args = spn.num_args - 1,
    .cmd = &cli->cmd
  };

  switch (spn_cli_parse(&parser)) {
    case SP_APP_CONTINUE: break;
    case SP_APP_QUIT: spn_cli_help(&parser); return SP_APP_QUIT;
    case SP_APP_ERR: {
      if (sp_str_valid(parser.err)) {
        sp_io_write_line(&spn.logger.err, parser.err);
      }

      spn_cli_help(&parser);
      return SP_APP_ERR;
    }
  }

  // Initialize verbosity from CLI flags
  if (cli->quiet) {
    spn.verbosity = SPN_VERBOSITY_QUIET;
  } else if (cli->verbose) {
    spn.verbosity = SPN_VERBOSITY_VERBOSE;
  } else {
    spn.verbosity = SPN_VERBOSITY_NORMAL;
  }

  if (sp_str_valid(cli->project_dir)) {
    spn.paths.project = sp_fs_canonicalize_path(cli->project_dir);
  }
  else {
    spn.paths.project = sp_str_copy(spn.paths.cwd);
  }
  spn.paths.manifest = sp_fs_join_path(spn.paths.project, sp_str_lit("spn.toml"));

  app = spn_app_new();
  spn_app_load(&app, spn.paths.manifest);

  switch (spn_cli_dispatch(&parser, cli)) {
    case SP_APP_CONTINUE: return SP_APP_CONTINUE;
    case SP_APP_QUIT: return SP_APP_QUIT;
    case SP_APP_ERR: return SP_APP_ERR;
  }

  sp_unreachable_return(SP_APP_ERR);
}

sp_app_result_t spn_poll(sp_app_t* sp) {
  spn_app_t* app = (spn_app_t*)sp->user_data;
  sp_da(spn_build_event_t) events = spn_event_buffer_drain(spn.events);

  sp_da_for(events, it) {
    spn_build_event_t* event = &events[it];

    // process anything we need to do special per-event
    switch (event->kind) {
      case SPN_EVENT_TARGET_BUILD: {
        spn_build_ctx_log(event->io, event->target.build.args);
        break;
      }
      case SPN_EVENT_RESOLVE: {
        sp_ht_for(app->resolver.resolved, it) {
          sp_str_t name = *sp_ht_it_getkp(app->resolver.resolved, it);
          spn_resolved_pkg_t resolved = *sp_ht_it_getp(app->resolver.resolved, it);
          spn_build_ctx_log(event->io, sp_format(
            "Resolved {} to version {}",
            SP_FMT_STR(resolved.pkg->name),
            SP_FMT_STR(spn_semver_to_str(resolved.version))
          ));
        }
        break;
      }
      case SPN_EVENT_ADD_TARGET: {
        spn.tui.info.max_name = sp_max(spn.tui.info.max_name, event->target.add.name.len);
        break;
      }
      default: {
        if (event->io) {
          spn_build_ctx_log(event->io, sp_format("event: {}", SP_FMT_STR(spn_build_event_kind_to_str(event->kind))));
        }
        break;
      }
    }

    // write to tui (filtered by verbosity)
    if (spn_build_event_get_verbosity(event->kind) <= spn.verbosity) {
      sp_io_write_line(&spn.logger.err, spn_tui_render_build_event(event, spn.tui.info.max_name));
    }
  }

  return SP_APP_CONTINUE;
}

sp_app_result_t spn_update(sp_app_t* sp) {
  spn_app_t* app = (spn_app_t*)sp->user_data;

  if (sp_atomic_s32_get(&sp->shutdown)) {
    return SP_APP_QUIT;
  }

  spn_task_executor_t* task = &app->tasks;
  s32 kind = task->data[task->index];
  spn_task_result_t result = SPN_TASK_DONE;

  // if (!task->initted) {
  //   spn_event_buffer_push_ex(spn.events, app->ev, spn_build_event_t event)
  // }

  switch (kind) {
    case SPN_TASK_KIND_NONE: {
      return SP_APP_QUIT;
    }
    case SPN_TASK_KIND_RESOLVE: {
      result = spn_task_resolve(app);
      break;
    }
    case SPN_TASK_KIND_SYNC: {
      if (!task->initted) spn_task_sync_init(app);
      result = spn_task_sync_update(app);
      break;
    }
    case SPN_TASK_KIND_CONFIGURE_V2: {
      if (!task->initted) spn_task_prepare_configure_graph(app);
      result = spn_task_update_configure_graph(app);
      break;
    }
    case SPN_TASK_KIND_PREPARE_BUILD_GRAPH_V2: {
      result = spn_task_prepare_build_graph_v2(app);
      break;
    }
    case SPN_TASK_KIND_RUN_BUILD_GRAPH: {
      if (!task->initted) spn_task_run_build_graph_init(app);
      result = spn_task_run_build_graph_update(app);
      break;
    }
    case SPN_TASK_KIND_RENDER_BUILD_GRAPH: {
      result = spn_task_graph(app);
      break;
    }
    case SPN_TASK_KIND_RUN: {
      result = spn_task_run_tests(app);
      break;
    }
    case SPN_TASK_KIND_GENERATE: {
      result = spn_task_generate(app);
      break;
    }
    case SPN_TASK_KIND_WHICH: {
      result = spn_task_which(app);
      break;
    }
    case SPN_TASK_KIND_PREPARE_BUILD_GRAPH: {
      result = SPN_TASK_DONE;
      break;
    }
    case SPN_TASK_KIND_COUNT: {
      SP_UNREACHABLE();
      break;
    }
  }

  task->initted = true;

  switch (result) {
    case SPN_TASK_ERROR: {
      spn_poll(sp);
      return SP_APP_ERR;
    }
    case SPN_TASK_CONTINUE: return SP_APP_CONTINUE;
    case SPN_TASK_DONE: {
      task->index++;
      task->initted = false;
      return SP_APP_CONTINUE;
    }
  }

  sp_unreachable_return(SP_APP_ERR);
}

sp_app_result_t spn_deinit(sp_app_t* sp) {
  spn_app_t* app = (spn_app_t*)sp->user_data;

  switch (spn.tui.mode) {
    case SPN_OUTPUT_MODE_INTERACTIVE: {
      // sp_tui_restore(&spn.tui);
      // sp_tui_show_cursor();
      // sp_tui_home();
      sp_tui_flush();
      break;
    }
    case SPN_OUTPUT_MODE_NONINTERACTIVE: {
      break;
    }
    case SPN_OUTPUT_MODE_QUIET: {
      break;
    }
    case SPN_OUTPUT_MODE_NONE: {
      break;
    }
  }

  if (!app->session.pkg) return SP_APP_QUIT;

  spn_pkg_unit_t* root = spn_session_find_root(&app->session);
  sp_om_for(app->session.units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(app->session.units.packages, it);

    sp_fs_create_sym_link(
      unit->ctx.paths.logs.build,
      sp_fs_join_path(root->ctx.paths.work, spn_build_ctx_get_build_log_name(&unit->ctx))
    );

    sp_om_for(unit->targets, t) {
      spn_target_unit_t* target = sp_om_at(unit->targets, t);
      sp_io_writer_close(&target->logs.build);
      sp_io_writer_close(&target->logs.test);
    }
    sp_io_writer_close(&unit->ctx.logs.build);
    sp_io_writer_close(&unit->ctx.logs.test);
  }

  sp_io_writer_close(&root->ctx.logs.build);
  sp_io_writer_close(&root->ctx.logs.test);

  return SP_APP_QUIT;
}

sp_app_config_t sp_main(s32 num_args, const c8** args) {
  spn = (spn_ctx_t) {
    .num_args = num_args,
    .args = args
  };
  app = SP_ZERO_STRUCT(spn_app_t);

  return (sp_app_config_t) {
    .user_data = &app,
    .on_init = spn_init,
    .on_poll = spn_poll,
    .on_update = spn_update,
    .on_deinit = spn_deinit,
    .fps = 144,
  };
}


/////////
// CLI //
/////////
spn_cli_command_info_t spn_cli_command_info_from_usage(spn_cli_command_usage_t cmd) {
  spn_cli_command_info_t info = {
    .name = sp_str_from_cstr(cmd.name),
    .usage = sp_str_from_cstr(cmd.usage),
    .summary = sp_str_from_cstr(cmd.summary),
  };

  // Process options
  sp_carr_for(cmd.opts, it) {
    if (!cmd.opts[it].name) break;

    spn_cli_opt_info_t opt = {
      .brief = sp_str_from_cstr(cmd.opts[it].brief),
      .name = sp_str_from_cstr(cmd.opts[it].name),
      .kind = cmd.opts[it].kind,
      .summary = sp_str_from_cstr(cmd.opts[it].summary),
      .placeholder = sp_str_from_cstr(cmd.opts[it].placeholder ? cmd.opts[it].placeholder : ""),
    };
    sp_da_push(info.opts, opt);
  }

  // Process arguments
  sp_carr_for(cmd.args, it) {
    if (!cmd.args[it].name) break;

    spn_cli_arg_info_t arg = {
      .name = sp_str_from_cstr(cmd.args[it].name),
      .kind = cmd.args[it].kind,
      .summary = sp_str_from_cstr(cmd.args[it].summary),
    };
    sp_da_push(info.args, arg);
    sp_da_push(info.brief, arg.name);
  }

  return info;
}

sp_str_t spn_cli_command_usage(spn_cli_command_usage_t cmd) {
  spn_cli_command_info_t info = spn_cli_command_info_from_usage(cmd);

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  SP_ASSERT(!sp_str_empty(info.summary));
  sp_str_builder_append_fmt(&builder, "{}", SP_FMT_CSTR(cmd.summary));
  sp_str_builder_new_line(&builder);

  if (!sp_dyn_array_empty(info.opts)) {
    sp_str_builder_new_line(&builder);

    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("options"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Short"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Long"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(info.opts, it) {
      spn_cli_opt_info_t opt = info.opts[it];

      // Build short flag display
      sp_str_t short_display;
      if (!sp_str_empty(opt.brief)) {
        sp_str_t short_text = sp_format("-{}", SP_FMT_STR(opt.brief));
        short_display = sp_format("{:fg brightyellow}", SP_FMT_STR(short_text));
      } else {
        short_display = sp_str_lit("");
      }

      // Build long flag display
      sp_str_t long_display;
      if (!sp_str_empty(opt.placeholder)) {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}={:fg white}", SP_FMT_STR(long_text), SP_FMT_STR(opt.placeholder));
      } else {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}", SP_FMT_STR(long_text));
      }

      sp_str_t kind_str = sp_format("{:fg brightblack}", SP_FMT_STR(spn_cli_opt_kind_to_str(opt.kind)));

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_str(&spn.tui.table, short_display);
      sp_tui_table_str(&spn.tui.table, long_display);
      sp_tui_table_str(&spn.tui.table, kind_str);
      sp_tui_table_str(&spn.tui.table, opt.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);

    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
  }

  if (!sp_dyn_array_empty(info.args)) {
    sp_str_builder_new_line(&builder);

    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("arguments"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Name"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(info.args, it) {
      spn_cli_arg_info_t arg = info.args[it];

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightyellow}", SP_FMT_STR(arg.name));
      sp_tui_table_str(&spn.tui.table, sp_str_lit("str"));
      sp_tui_table_str(&spn.tui.table, arg.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_t table = sp_tui_table_render(&spn.tui.table);

    sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(table));
  }

  return sp_str_builder_to_str(&builder);
}

sp_str_t spn_cli_usage(spn_cli_command_usage_t* cmd) {
  spn_cli_command_info_t cmd_info = spn_cli_command_info_from_usage(*cmd);
  spn_cli_usage_info_t info = SP_ZERO_INITIALIZE();

  // Collect subcommands
  if (cmd->commands) {
    for (spn_cli_command_usage_t* sub = cmd->commands; sub->name; sub++) {
      spn_cli_command_info_t sub_info = spn_cli_command_info_from_usage(*sub);
      sp_dyn_array_push(info.commands, sub_info);
    }
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  if (cmd->summary) {
    sp_str_builder_append_fmt(&builder, "{}", SP_FMT_CSTR(cmd->summary));
    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
  }

  if (cmd->usage) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("usage"));
    sp_str_builder_indent(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "{:fg brightcyan}", SP_FMT_CSTR(cmd->usage));
    sp_str_builder_dedent(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
  }

  // Render opts (from the command itself)
  if (!sp_dyn_array_empty(cmd_info.opts)) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("options"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Short"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Long"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(cmd_info.opts, it) {
      spn_cli_opt_info_t opt = cmd_info.opts[it];

      sp_str_t short_display;
      if (!sp_str_empty(opt.brief)) {
        sp_str_t short_text = sp_format("-{}", SP_FMT_STR(opt.brief));
        short_display = sp_format("{:fg brightyellow}", SP_FMT_STR(short_text));
      } else {
        short_display = sp_str_lit("");
      }

      sp_str_t long_display;
      if (!sp_str_empty(opt.placeholder)) {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}={:fg white}", SP_FMT_STR(long_text), SP_FMT_STR(opt.placeholder));
      } else {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}", SP_FMT_STR(long_text));
      }

      sp_str_t kind_str = sp_format("{:fg brightblack}", SP_FMT_STR(spn_cli_opt_kind_to_str(opt.kind)));

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_str(&spn.tui.table, short_display);
      sp_tui_table_str(&spn.tui.table, long_display);
      sp_tui_table_str(&spn.tui.table, kind_str);
      sp_tui_table_str(&spn.tui.table, opt.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
    sp_str_builder_new_line(&builder);
  }

  // Render subcommands
  if (!sp_dyn_array_empty(info.commands)) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("commands"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Command"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Arguments"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(info.commands, it) {
      spn_cli_command_info_t command = info.commands[it];
      sp_str_t args = sp_str_join_n(command.brief, sp_dyn_array_size(command.brief), sp_str_lit(", "));

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightcyan}", SP_FMT_STR(command.name));
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightyellow}", SP_FMT_STR(args));
      sp_tui_table_str(&spn.tui.table, command.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
  }

  return sp_str_builder_to_str(&builder);
}

sp_app_result_t spn_cli_help(spn_cli_parser_t* p) {
  if (!p->resolved) {
    sp_log(spn_cli_command_usage(*p->cmd));
    return SP_APP_QUIT;
  }
  else if (!p->resolved->commands) {
    sp_log(spn_cli_command_usage(*p->resolved));
    return SP_APP_QUIT;
  }
  else {
    sp_log(spn_cli_command_usage(*p->resolved->commands));
    return SP_APP_QUIT;
  }
  return SP_APP_QUIT;
}



/////////
// CLI //
/////////
sp_app_result_t spn_cli_set_profile(spn_app_t* app, sp_str_t name) {
  if (sp_str_empty(name)) {
    app->config.profile = spn_pkg_get_default_profile(&app->package);
    return SP_APP_CONTINUE;
  }

  if (!sp_om_has(app->package.profiles, name)) {
    spn_log_error("{:fg brightcyan} profile isn't defined in {:fg brightcyan}",
      SP_FMT_STR(name),
      SP_FMT_STR(app->package.paths.manifest)
    );
    return SP_APP_ERR;
  }

  app->config.profile = spn_pkg_get_profile_or_default(&app->package, name);
  return SP_APP_CONTINUE;
}

spn_pkg_unit_t* spn_cli_assert_unit_exists(sp_str_t name) {
  spn_pkg_unit_t* dep = sp_om_get(app.session.units.packages, name);
  SP_ASSERT_FMT(dep, "{:fg brightyellow} is not in this project", SP_FMT_STR(name));
  return dep;
}

// Get resolved package path from resolver (doesn't require builder init)
sp_str_t spn_cli_get_resolved_pkg_source(sp_str_t name) {
  spn_resolved_pkg_t* resolved = sp_ht_getp(app.resolver.resolved, name);
  SP_ASSERT_FMT(resolved, "{:fg brightyellow} is not in this project", SP_FMT_STR(name));
  return sp_fs_join_path(spn.paths.source, resolved->pkg->name);
}

sp_app_result_t spn_cli_init(spn_cli_t* cli) {
  spn_cli_init_t* cmd = &cli->init;

  spn_app_t app = spn_app_init_and_write(
    spn.paths.cwd,
    sp_fs_get_stem(spn.paths.cwd),
    cmd->bare ? SPN_APP_INIT_BARE : SPN_APP_INIT_NORMAL
  );

  SP_LOG("Initialized project {:fg brightcyan}. Run {:fg brightyellow} to build.", SP_FMT_STR(app.package.name), SP_FMT_CSTR("spn build"));
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_root(spn_cli_t* cli) {
  sp_str_t help = spn_cli_usage(&cli->cmd);
  sp_log(help);
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_list(spn_cli_t* cli) {
  sp_tui_begin_table(&spn.tui.table);
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Package"));
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Version"));
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Repo"));
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Author"));
  sp_tui_table_header_row(&spn.tui.table);

  sp_ht_for_kv(app.registry, it) {
    spn_pkg_t* package = spn_app_ensure_package(&app, (spn_pkg_req_t) {
      .name = sp_fs_get_stem(*it.val),
      .kind = SPN_PACKAGE_KIND_INDEX
    });

    sp_tui_table_next_row(&spn.tui.table);
    sp_tui_table_fmt(&spn.tui.table, "{:fg brightcyan}", SP_FMT_STR(package->name));
    sp_tui_table_str(&spn.tui.table, spn_semver_to_str(package->version));
    sp_tui_table_str(&spn.tui.table, sp_str_truncate(package->repo, 50, sp_str_lit("...")));
    sp_tui_table_str(&spn.tui.table, sp_str_truncate(package->author, 30, sp_str_lit("...")));
  }

  sp_tui_table_end(&spn.tui.table);
  sp_log(sp_tui_table_render(&spn.tui.table));
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_clean(spn_cli_t* cli) {
  spn_cli_clean_t* cmd = &cli->clean;

  // Create a minimal context for clean events
  spn_build_ctx_t ctx = SP_ZERO_INITIALIZE();
  ctx.name = sp_str_lit("package");

  sp_str_t build_dir = sp_fs_join_path(app.paths.dir, sp_str_lit("build"));

  if (sp_str_valid(cmd->profile)) {
    // Clean only the specified profile
    sp_str_t profile_dir = sp_fs_join_path(build_dir, cmd->profile);
    if (sp_fs_exists(profile_dir)) {
      spn_event_buffer_push_ctx(spn.events, &ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_CLEAN,
        .clean.path = profile_dir
      });
      sp_fs_remove_dir(profile_dir);
    }
  } else {
    // Clean the entire build directory
    if (sp_fs_exists(build_dir)) {
      spn_event_buffer_push_ctx(spn.events, &ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_CLEAN,
        .clean.path = build_dir
      });
      sp_fs_remove_dir(build_dir);
    }

    // Remove the lock file
    if (sp_fs_exists(app.paths.lock)) {
      spn_event_buffer_push_ctx(spn.events, &ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_CLEAN,
        .clean.path = app.paths.lock
      });
      sp_fs_remove_file(app.paths.lock);
    }
  }

  // Drain and render events
  sp_da(spn_build_event_t) events = spn_event_buffer_drain(spn.events);
  sp_da_for(events, it) {
    spn_build_event_t* event = &events[it];
    sp_io_write_line(&spn.logger.err, spn_tui_render_build_event(event, spn.tui.info.max_name));
  }

  return SP_APP_QUIT;
}


sp_app_result_t spn_cli_copy(spn_cli_t* cli) {
  spn_cli_copy_t* cmd = &cli->copy;

  sp_str_t destination = sp_fs_normalize_path(cmd->directory);
  sp_str_t to = sp_fs_join_path(spn.paths.cwd, destination);
  sp_fs_create_dir(to);

  sp_om_for(app.session.units.packages, it) {
    spn_pkg_unit_t* dep = sp_om_at(app.session.units.packages, it);
    spn_build_ctx_t* ctx = &dep->ctx;

    sp_dyn_array(sp_os_dir_ent_t) entries = sp_fs_collect(ctx->paths.lib);
    sp_dyn_array_for(entries, i) {
      sp_os_dir_ent_t* entry = entries + i;
      sp_fs_copy_file(
        entry->file_path,
        sp_fs_join_path(to, sp_fs_get_name(entry->file_path))
      );
    }
  }
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_ls(spn_cli_t* cli) {
  spn_cli_ls_t* cmd = &cli->ls;

  spn_app_resolve(&app);

  if (sp_str_valid(cmd->package)) {
    sp_str_t dir = spn_cli_get_resolved_pkg_source(cmd->package);
    sp_sh_ls(dir);
  }
  else {
    spn_pkg_dir_t kind = SPN_DIR_CACHE;
    if (sp_str_valid(cmd->dir)) {
      kind = spn_cache_dir_kind_from_str(cmd->dir);
    }

    sp_str_t dir = spn_cache_dir_kind_to_path(kind);
    sp_sh_ls(dir);
  }
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_which(spn_cli_t* cli) {
  sp_try(spn_cli_set_profile(&app, sp_str_lit("")));

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE_V2);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_WHICH);
  return SP_APP_CONTINUE;
}

sp_app_result_t spn_cli_manifest(spn_cli_t* cli) {
  spn_cli_manifest_t* cmd = &cli->manifest;

  spn_app_resolve(&app);

  spn_pkg_unit_t* dep = spn_cli_assert_unit_exists(cmd->package);

  sp_str_t path = dep->ctx.pkg->paths.manifest;
  sp_str_t manifest = sp_io_read_file(path);
  if (!sp_str_valid(manifest)) {
    SP_FATAL("Failed to read manifest at {:fg brightyellow}", SP_FMT_STR(path));
  }

  sp_log(manifest);
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_graph(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  app.config = (spn_app_config_t) {
    .force = command->force,
    .filter = (spn_target_filter_t) {
      .name = command->target,
      .disabled = {
        .public = false,
        .test = false,
      }
    },
  };

  sp_try(spn_cli_set_profile(&app, command->profile));

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE_V2);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_PREPARE_BUILD_GRAPH_V2);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RENDER_BUILD_GRAPH);

  return SP_APP_CONTINUE;
}

sp_app_result_t spn_cli_add(spn_cli_t* cli) {
  spn_cli_add_t* cmd = &cli->add;

  if (cmd->test && cmd->build) {
    SP_FATAL("cannot specify both {:fg yellow} and {:fg yellow}", SP_FMT_CSTR("--test"), SP_FMT_CSTR("--build"));
  }

  spn_visibility_t visibility = cmd->test  ? SPN_VISIBILITY_TEST
                              : cmd->build ? SPN_VISIBILITY_BUILD
                              :              SPN_VISIBILITY_PUBLIC;
  if (sp_ht_getp(app.package.deps, cmd->package)) {
    SP_FATAL("{:fg brightyellow} is already in your project", SP_FMT_STR(cmd->package));
  }
  spn_pkg_add_dep_latest(&app.package, cmd->package, visibility);
  spn_app_resolve_from_solver(&app);
  spn_app_update_lock_file(&app);
  spn_app_write_manifest(&app.package, app.package.paths.manifest);
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_update(spn_cli_t* cli) {
  spn_cli_update_t* cmd = &cli->update;

  spn_pkg_req_t* existing = sp_ht_getp(app.package.deps, cmd->package);
  if (!existing) {
    SP_FATAL("package {:fg brightcyan} is not a dependency", SP_FMT_STR(cmd->package));
  }

  spn_pkg_add_dep_latest(&app.package, cmd->package, existing->visibility);
  spn_app_resolve_from_solver(&app);
  spn_app_update_lock_file(&app);
  spn_app_write_manifest(&app.package, app.package.paths.manifest);
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_tool_install(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

sp_app_result_t spn_cli_tool_uninstall(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

sp_app_result_t spn_cli_tool_run(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

sp_app_result_t spn_cli_tool(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

sp_app_result_t spn_cli_generate(spn_cli_t* cli) {
  spn_cli_generate_t* command = &cli->generate;

  if (sp_str_valid(command->path) && !sp_str_valid(command->generator)) {
    SP_FATAL(
      "output path was specified, but no generator. try e.g.:\n  spn generate --path {} {:fg yellow}",
      SP_FMT_STR(command->path),
      SP_FMT_CSTR("--generator make")
    );
  }
  if (!sp_str_valid(command->generator)) command->generator = sp_str_lit("");
  if (!sp_str_valid(command->compiler)) command->compiler = sp_str_lit("gcc");

  if (!app.lock.some) {
    SP_FATAL("No lock file found. Run {:fg yellow} first.", SP_FMT_CSTR("spn build"));
  }

  sp_try(spn_cli_set_profile(&app, sp_str_lit("")));

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE_V2);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_GENERATE);

  return SP_APP_CONTINUE;
}

sp_app_result_t spn_cli_test(spn_cli_t* cli) {
  spn_cli_test_t* command = &cli->test;

  app.config = (spn_app_config_t) {
    .filter = (spn_target_filter_t) {
      .name = command->target,
      .disabled = {
        .public = true
      }
    }
  };

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE_V2);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_PREPARE_BUILD_GRAPH_V2);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN);

  sp_try_as(spn_cli_set_profile(&app, command->profile), SP_APP_ERR);

  return SP_APP_CONTINUE;
}

sp_app_result_t spn_cli_build(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  app.config = (spn_app_config_t) {
    .force = command->force,
    .filter = (spn_target_filter_t) {
      .name = command->target,
      .disabled = {
        .test = sp_str_empty(command->target) && !command->tests
      }
    },
  };

  sp_try(spn_cli_set_profile(&app, command->profile));

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE_V2);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_PREPARE_BUILD_GRAPH_V2);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN_BUILD_GRAPH);

  return SP_APP_CONTINUE;
}

spn_cli_command_usage_t spn_cli() {
  spn_cli_t* cli = &spn.cli;
  static spn_cli_command_usage_t tools [] = {
    {
      .name = "install",
      .opts = {
        {
          .brief = "f",
          .name = "force",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Force reinstall even if already installed",
          .ptr = &spn.cli.tool.install.force
        },
        {
          .brief = "v",
          .name = "version",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Version to install",
          .placeholder = "VERSION",
          .ptr = &spn.cli.tool.install.version
        }
      },
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package to install",
          .ptr = &spn.cli.tool.install.package
        }
      },
      .summary = "Install a package's binary targets to the PATH",
      .handler = spn_cli_tool_install
    },
    {
      .name = "uninstall",
      .opts = {
        {
          .brief = "f",
          .name = "force",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Force removal even if not installed by spn",
          .ptr = &spn.cli.tool.install.force
        }
      },
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package to uninstall",
          .ptr = &spn.cli.tool.install.package
        }
      },
      .summary = "Uninstall a package's binary targets from the PATH",
      .handler = spn_cli_tool_uninstall
    },
    {
      .name = "run",
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package to run",
          .ptr = &spn.cli.tool.run.package
        },
        {
          .name = "command",
          .kind = SPN_CLI_ARG_KIND_OPTIONAL,
          .summary = "The command to run",
          .ptr = &spn.cli.tool.run.command
        }
      },
      .summary = "Run a binary from a package",
      .handler = spn_cli_tool_run
    },
    SPN_CLI_ARGS_DONE,
  };

  static spn_cli_command_usage_t commands [] = {
    {
      .name = "init",
      .handler = spn_cli_init,
      .summary = "Initialize a project in the current directory",
      .opts = {
        {
          .brief = "b",
          .name = "bare",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Create minimal project without sp dependency or main.c",
          .ptr = &spn.cli.init.bare
        }
      },
    },
    {
      .name = "add",
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package to add",
          .ptr = &spn.cli.add.package
        }
      },
      .opts = {
        {
          .brief = "t",
          .name = "test",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Add as a test dependency",
          .ptr = &spn.cli.add.test
        },
        {
          .brief = "b",
          .name = "build",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Add as a build dependency",
          .ptr = &spn.cli.add.build
        }
      },
      .summary = "Add the latest version of a package to the project",
      .handler = spn_cli_add
    },

    {
      .name = "build",
      .opts = {
        {
          .brief = "f",
          .name = "force",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Force build, even if it exists in store",
          .ptr = &spn.cli.build.force
        },
        {
          .brief = "p",
          .name = "profile",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Profile to use for building",
          .placeholder = "PROFILE",
          .ptr = &spn.cli.build.profile
        },
        {
          .brief = "t",
          .name = "target",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Target to build",
          .placeholder = "TARGET",
          .ptr = &spn.cli.build.target
        },
        {
          .name = "tests",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Include test targets",
          .ptr = &spn.cli.build.tests
        }
      },
      .summary = "Build the project, including dependencies, from source",
      .handler = spn_cli_build
    },

    {
      .name = "test",
      .opts = {
        {
          .brief = "p",
          .name = "profile",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Profile to use for building",
          .placeholder = "PROFILE",
          .ptr = &spn.cli.test.profile
        },
        {
          .brief = "t",
          .name = "target",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Test target to run",
          .placeholder = "TARGET",
          .ptr = &spn.cli.test.target
        }
      },
      .summary = "Build and run tests",
      .handler = spn_cli_test
    },

    {
      .name = "generate",
      .opts = {
        {
          .brief = "g",
          .name = "generator",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Generator type (raw, shell, make)",
          .placeholder = "TYPE",
          .ptr = &spn.cli.generate.generator
        },
        {
          .brief = "c",
          .name = "compiler",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Compiler to format flags for (gcc, clang, tcc)",
          .placeholder = "COMPILER",
          .ptr = &spn.cli.generate.compiler
        },
        {
          .brief = "p",
          .name = "path",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Output directory for generated file",
          .placeholder = "PATH",
          .ptr = &spn.cli.generate.path
        }
      },
      .summary = "Generate build system files with dependency flags",
      .handler = spn_cli_generate
    },
    {
      .name = "link",
      .handler = spn_cli_copy,
      .args = {
        {
          .name = "kind",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The link kind",
          .ptr = &spn.cli.copy.directory
        }
      },
      .summary = "Link or copy the binary outputs of your dependencies",
    },
    {
      .name = "update",
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package to update",
          .ptr = &spn.cli.update.package
        }
      },
      .summary = "Update an existing package to the latest version in the project",
      .handler = spn_cli_update
    },
    {
      .name = "list",
      .summary = "List all known packages in all registries",
      .handler = spn_cli_list
    },
    {
      .name = "which",
      .opts = {
        {
          .brief = "d",
          .name = "dir",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Which directory to show (store, include, lib, source, work, vendor)",
          .placeholder = "DIR",
          .ptr = &spn.cli.which.dir
        }
      },
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_OPTIONAL,
          .summary = "The package to show path for",
          .ptr = &spn.cli.which.package
        }
      },
      .summary = "Print the absolute path of a cache dir for a package",
      .handler = spn_cli_which
    },
    {
      .name = "ls",
      .opts = {
        {
          .brief = "d",
          .name = "dir",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Which directory to list (store, include, lib, source, work, vendor)",
          .placeholder = "DIR",
          .ptr = &spn.cli.ls.dir
        }
      },
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_OPTIONAL,
          .summary = "The package to list",
          .ptr = &spn.cli.ls.package
        }
      },
      .summary = "Run ls against a cache dir for a package (e.g. to see build output)",
      .handler = spn_cli_ls
    },
    {
      .name = "manifest",
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package name",
          .ptr = &spn.cli.manifest.package
        }
      },
      .summary = "Print the full manifest source for a package",
      .handler = spn_cli_manifest
    },
    {
      .name = "graph",
      .opts = {
        {
          .brief = "o",
          .name = "output",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Output file path (stdout if not specified)",
          .placeholder = "FILE",
          .ptr = &spn.cli.graph.output
        },
        {
          .brief = "d",
          .name = "dirty",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Color nodes by dirtiness instead of type",
          .ptr = &spn.cli.graph.dirty
        }
      },
      .summary = "Output the build graph as mermaid",
      .handler = spn_cli_graph
    },
    {
      .name = "tool",
      .summary = "Run, install, and manage binaries defined by spn packages",
      .handler = spn_cli_tool,
      .commands = tools
    },
    {
      .name = "clean",
      .summary = "Remove build directory and lock file",
      .opts = {
        {
          .brief = "p",
          .name = "profile",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Only clean the specified profile",
          .placeholder = "PROFILE",
          .ptr = &spn.cli.clean.profile
        }
      },
      .handler = spn_cli_clean
    },
    SPN_CLI_ARGS_DONE,
  };

  return (spn_cli_command_usage_t) {
    .name = "spn",
    .handler = spn_cli_root,
    .summary = "A package manager and build tool for modern C",
    .opts = {
      {
        .brief = "h",
        .name = "help",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Print help message",
        .ptr = &spn.cli.help
      },
      {
        .brief = "C",
        .name = "project-dir",
        .kind = SPN_CLI_OPT_KIND_STRING,
        .summary = "Specify the directory containing project file",
        .placeholder = "DIR",
        .ptr = &spn.cli.project_dir
      },
      {
        .brief = "f",
        .name = "file",
        .kind = SPN_CLI_OPT_KIND_STRING,
        .summary = "Specify the project file path",
        .placeholder = "FILE",
        .ptr = &spn.cli.project_file
      },
      {
        .brief = "o",
        .name = "output",
        .kind = SPN_CLI_OPT_KIND_STRING,
        .summary = "Output mode: interactive, noninteractive, quiet, none",
        .placeholder = "MODE",
        .ptr = &spn.cli.output
      },
      {
        .brief = "v",
        .name = "verbose",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Show verbose output",
        .ptr = &spn.cli.verbose
      },
      {
        .brief = "q",
        .name = "quiet",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Only show errors",
        .ptr = &spn.cli.quiet
      }
    },
    .commands = commands
  };

}
