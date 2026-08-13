#include "unit/unit.h"

#include "ctx/types.h"

void spn_unit_paths_init(spn_pkg_unit_t* unit, spn_loaded_pkg_t* loaded) {
  spn_session_t* s = unit->session;
  sp_mem_t mem = s->mem;
  spn_build_unit_t* build = unit->build;

  unit->paths.manifest = loaded->paths.manifest;
  unit->paths.script = loaded->paths.script;
  unit->paths.roots = loaded->roots;

  switch (loaded->source) {
    case SPN_PKG_SOURCE_ROOT:
    case SPN_PKG_SOURCE_FILE: {
      sp_str_t work = sp_fs_join_path(mem, build->paths.root, sp_str_lit(".spn"));
      sp_str_t store = sp_fs_join_path(mem, build->paths.root, sp_str_lit("store"));
      unit->paths.work = sp_fs_join_path(mem, work, loaded->info->name);
      unit->paths.store = sp_fs_join_path(mem, store, loaded->info->name);
      break;
    }
    case SPN_PKG_SOURCE_INDEX: {
      sp_str_t fingerprint = spn_unit_fingerprint_str(mem, spn_unit_fingerprint(s, build, unit->id.pkg));
      sp_str_t work = sp_fs_join_path(mem, s->ctx->paths.caches.build.dir, loaded->info->qualified);
      sp_str_t store = sp_fs_join_path(mem, s->ctx->paths.caches.store.dir, loaded->info->qualified);
      unit->paths.work = sp_fs_join_path(mem, work, fingerprint);
      unit->paths.store = sp_fs_join_path(mem, store, fingerprint);
      break;
    }
  }

  unit->paths.include = sp_fs_join_path(mem, unit->paths.store, SP_LIT("include"));
  unit->paths.bin = sp_fs_join_path(mem, unit->paths.store, SP_LIT("bin"));
  unit->paths.lib = sp_fs_join_path(mem, unit->paths.store, SP_LIT("lib"));
  unit->paths.vendor = sp_fs_join_path(mem, unit->paths.store, SP_LIT("vendor"));

  unit->paths.generated = unit->paths.work;
  unit->paths.object = sp_fs_join_path(mem, unit->paths.generated, SP_LIT("object"));

  unit->paths.stamp.dir = sp_fs_join_path(mem, unit->paths.generated, SP_LIT("stamp"));
  unit->paths.stamp.configure = sp_fs_join_path(mem, unit->paths.stamp.dir, SP_LIT("configure.stamp"));
  unit->paths.stamp.package = sp_fs_join_path(mem, unit->paths.stamp.dir, SP_LIT("package.stamp"));
}
