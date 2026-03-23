#include "unit/package.h"

#include "app/app.h"
#include "ctx/ctx.h"
#include "event/event.h"
#include "external/cc.h"
#include "external/git.h"
#include "external/tom.h"
#include "log.h"
#include "pkg/pkg.h"
#include "target/target.h"
#include "unit/build.h"

#include <setjmp.h>

sp_str_t spn_pkg_unit_get_include_dir(spn_pkg_unit_t* unit) {
  return spn_build_ctx_get_include_dir(&unit->ctx);
}

sp_str_t spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node) {
  return sp_fs_join_path(ctx->paths.stamp.dir, node->tag);
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

spn_err_t spn_pkg_unit_sync_remote(spn_pkg_unit_t* ctx) {
  if (!sp_fs_exists(ctx->ctx.paths.source)) {
    sp_str_t url = spn_pkg_get_url(ctx->ctx.pkg);
    sp_try(spn_git_clone(url, ctx->ctx.paths.source));
  }
  else {
    sp_try(spn_git_fetch(ctx->ctx.paths.source));
  }

  return SPN_OK;
}

spn_err_t spn_pkg_unit_sync_local(spn_pkg_unit_t* ctx) {
  return spn_git_checkout(ctx->ctx.paths.source, ctx->metadata.commit);
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

spn_err_t spn_pkg_unit_call_hook(spn_pkg_unit_t* ctx, spn_build_fn_t fn) {
  jmp_buf jump;
  int status = tcc_setjmp(ctx->tcc, jump, fn);
  if (!status) {
    fn(&ctx->ctx);
  }
  else {
    // @spader @log
    // What else can we get from TCC here?
    spn_event_buffer_push_ctx(spn.events, &ctx->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CRASHED,
      .crashed.path = ctx->ctx.pkg->paths.script,
    });
    return SPN_ERROR;
  }

  return SPN_OK;
}

void spn_pkg_unit_add_target(spn_pkg_unit_t* pkg, spn_target_t* target) {
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
  unit->paths.logs.jsonl = sp_fs_join_path(unit->paths.work, sp_format("{}.build.jsonl", SP_FMT_STR(target->name)));

  sp_fs_create_dir(unit->paths.work);
  sp_fs_create_dir(unit->paths.generated);
  sp_fs_create_dir(unit->paths.object);
  sp_fs_create_dir(unit->paths.store);
  sp_fs_create_dir(unit->paths.bin);
  sp_fs_create_dir(unit->paths.include);
  sp_fs_create_dir(unit->paths.lib);
  sp_fs_create_dir(unit->paths.vendor);
  sp_fs_create_file(unit->paths.logs.build);
  sp_fs_create_file(unit->paths.logs.jsonl);

  unit->logs.build = sp_io_writer_from_file(unit->paths.logs.build, SP_IO_WRITE_MODE_APPEND);
  unit->logs.jsonl = sp_io_writer_from_file(unit->paths.logs.jsonl, SP_IO_WRITE_MODE_APPEND);
}
