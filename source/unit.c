#include "app.h"

#include "cc.h"
#include "external/git.h"

#include "sp/io.h"

#include <setjmp.h>

spn_user_node_t* spn_find_user_node(spn_node_t node) {
  SP_ASSERT(node.index < sp_da_size(node.ctx->nodes.all));
  return &node.ctx->nodes.all[node.index];
}

spn_node_t spn_add_node(spn_build_ctx_t* c, const c8* tag) {
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

sp_str_t spn_build_ctx_resolve_dir(const spn_build_ctx_t* b, spn_pkg_dir_t kind, sp_str_t sub) {
  return sp_fs_join_path(spn_build_ctx_get_dir(b, kind), sub);
}

sp_str_t spn_build_ctx_get_dir(const spn_build_ctx_t* b, spn_pkg_dir_t kind) {
  switch (kind) {
    case SPN_DIR_STORE: {
      return b->paths.store;
    }
    case SPN_DIR_INCLUDE: {
      return b->paths.include;
    }
    case SPN_DIR_LIB: {
      return b->paths.lib;
    }
    case SPN_DIR_VENDOR: {
      return b->paths.vendor;
    }
    case SPN_DIR_SOURCE: {
      return b->paths.source;
    }
    case SPN_DIR_WORK: {
      return b->paths.work;
    }
    case SPN_DIR_CACHE: {
      return b->paths.store;
    }
    case SPN_DIR_PROJECT: {
      return spn.paths.project;
    }
    case SPN_DIR_NONE: {
      return sp_str_lit("");
    }
  }

  SP_UNREACHABLE();
  return sp_str_lit("");
}

sp_str_t spn_build_ctx_get_include_dir(spn_build_ctx_t* build) {
  return build->paths.include;
}

sp_str_t spn_build_ctx_get_lib_dir(spn_build_ctx_t* build) {
  switch (build->linkage) {
    case SPN_LIB_KIND_SHARED: {
      return build->paths.lib;
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
        if (sp_str_empty(var.key)) {
          break;
        }

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

spn_err_t spn_pkg_unit_call_hook(spn_pkg_unit_t* ctx, spn_build_fn_t fn) {
  jmp_buf jump;
  int status = tcc_setjmp(ctx->tcc, jump, fn);
  if (!status) {
    fn(&ctx->ctx);
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

void spn_pkg_unit_add_target(spn_pkg_unit_t* pkg, spn_target_t* target) {
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
