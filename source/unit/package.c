#include "unit/package.h"

#include "app/app.h"
#include "ctx/ctx.h"
#include "err.h"
#include "event/event.h"
#include "external/cc.h"
#include "external/git.h"
#include "external/tom.h"
#include "log/log.h"
#include "pkg/pkg.h"
#include "target/target.h"
#include "unit/build.h"
#include "unit/types.h"

#include <setjmp.h>

sp_str_t spn_pkg_unit_get_include_dir(spn_pkg_unit_t* unit) {
  return spn_build_ctx_get_include_dir(&unit->ctx);
}

sp_str_t spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node) {
  return sp_fs_join_path(ctx->paths.stamp.dir, node->tag);
}

void spn_pkg_unit_init(spn_pkg_unit_t* unit, spn_pkg_unit_config_t config) {
  unit->ctx.arena = sp_mem_arena_new_ex(256, SP_MEM_ARENA_MODE_NO_REALLOC, 1);

  unit->ctx.pkg = config.ctx.package;
  unit->ctx.session = config.ctx.session;
  unit->ctx.paths.source = config.ctx.paths.source;
  unit->ctx.paths.store = config.ctx.paths.store;
  unit->ctx.paths.include = sp_fs_join_path(unit->ctx.paths.store, SP_LIT("include"));
  unit->ctx.paths.bin = sp_fs_join_path(unit->ctx.paths.store, SP_LIT("bin"));
  unit->ctx.paths.lib = sp_fs_join_path(unit->ctx.paths.store, SP_LIT("lib"));
  unit->ctx.paths.vendor = sp_fs_join_path(unit->ctx.paths.store, SP_LIT("vendor"));

  unit->ctx.paths.work = config.ctx.paths.work;
  unit->ctx.paths.generated = sp_fs_join_path(unit->ctx.paths.work, SP_LIT("spn"));

  unit->logs.build = sp_format("{}.build.log", SP_FMT_STR(unit->pkg->name));
  unit->logs.test = sp_format("{}.test.log", SP_FMT_STR(unit->pkg->name));
  unit->logs.jsonl = sp_format("{}.jsonl", SP_FMT_STR(unit->pkg->name));

  unit->paths.logs.build = sp_fs_join_path(unit->ctx.paths.work, unit->logs.build);
  unit->paths.logs.test = sp_fs_join_path(unit->ctx.paths.work, unit->logs.test);
  unit->paths.logs.jsonl = sp_fs_join_path(unit->ctx.paths.work, unit->logs.jsonl);

  sp_fs_create_dir(unit->ctx.paths.work);
  sp_fs_create_dir(unit->ctx.paths.generated);
  sp_fs_create_dir(unit->ctx.paths.store);
  sp_fs_create_dir(unit->ctx.paths.bin);
  sp_fs_create_dir(unit->ctx.paths.include);
  sp_fs_create_dir(unit->ctx.paths.lib);
  sp_fs_create_dir(unit->ctx.paths.vendor);
  sp_fs_create_file(unit->paths.logs.build);
  sp_fs_create_file(unit->paths.logs.jsonl);

  unit->logs.io.build = sp_io_writer_from_file(unit->paths.logs.build, SP_IO_WRITE_MODE_APPEND);
  unit->logs.io.jsonl = sp_io_writer_from_file(unit->paths.logs.jsonl, SP_IO_WRITE_MODE_APPEND);

  unit->paths.stamp.dir = sp_fs_join_path(unit->ctx.paths.generated, SP_LIT("stamp"));
  unit->paths.stamp.main = sp_fs_join_path(unit->paths.stamp.dir, SP_LIT("main.stamp"));
  unit->paths.stamp.exit = sp_fs_join_path(unit->paths.stamp.dir, SP_LIT("user.stamp"));
  unit->paths.stamp.configure = sp_fs_join_path(unit->paths.stamp.dir, SP_LIT("configure.stamp"));
  unit->paths.stamp.build = sp_fs_join_path(unit->paths.stamp.dir, SP_LIT("build.stamp"));
  unit->paths.stamp.package = sp_fs_join_path(unit->paths.stamp.dir, SP_LIT("package.stamp"));

  sp_fs_create_dir(unit->paths.stamp.dir);
}

void spn_pkg_unit_write_stamp(spn_pkg_unit_t* unit, sp_str_t path) {
  sp_fs_create_file_str(path, unit->pkg->name);
}

spn_err_t spn_pkg_unit_call_hook(spn_pkg_unit_t* unit, spn_build_fn_t fn) {
  jmp_buf jump;
  int status = tcc_setjmp(unit->tcc, jump, fn);
  if (!status) {
    fn(&unit->ctx);
  }
  else {
    // @spader @log
    // What else can we get from TCC here?
    spn_event_buffer_push_ctx(spn.events, &unit->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CRASHED,
      .crashed.path = sp_str_lit("")
    });
    return SPN_ERROR;
  }

  return SPN_OK;
}
