#include "unit/unit.h"

#include "log/lazy/lazy.h"

void spn_unit_paths_init(spn_pkg_unit_t* unit, spn_loaded_pkg_t* loaded) {
  spn_session_t* s = unit->session;
  sp_mem_t mem = s->mem;
  spn_build_unit_t* build = unit->build;

  unit->paths.manifest = loaded->paths.manifest;
  unit->paths.script = loaded->paths.script;
  unit->paths.recipe = loaded->roots.recipe;
  unit->paths.source = loaded->roots.source;

  switch (loaded->source) {
    case SPN_PKG_SOURCE_ROOT:
    case SPN_PKG_SOURCE_FILE: {
      sp_str_t work = sp_fs_join_path(mem, build->paths.root, sp_str_lit("work"));
      sp_str_t store = sp_fs_join_path(mem, build->paths.root, sp_str_lit("store"));
      unit->paths.work = sp_fs_join_path(mem, work, loaded->info->name);
      unit->paths.store = sp_fs_join_path(mem, store, loaded->info->name);
      break;
    }
    case SPN_PKG_SOURCE_INDEX: {
      sp_str_t fingerprint = spn_unit_fingerprint_str(mem, spn_unit_fingerprint(s, build, unit->id.pkg));
      sp_str_t work = sp_fs_join_path(mem, s->paths.system.caches.build.dir, loaded->info->qualified);
      sp_str_t store = sp_fs_join_path(mem, s->paths.system.caches.store.dir, loaded->info->qualified);
      unit->paths.work = sp_fs_join_path(mem, work, fingerprint);
      unit->paths.store = sp_fs_join_path(mem, store, fingerprint);
      break;
    }
  }

  unit->paths.include = sp_fs_join_path(mem, unit->paths.store, SP_LIT("include"));
  unit->paths.bin = sp_fs_join_path(mem, unit->paths.store, SP_LIT("bin"));
  unit->paths.lib = sp_fs_join_path(mem, unit->paths.store, SP_LIT("lib"));
  unit->paths.vendor = sp_fs_join_path(mem, unit->paths.store, SP_LIT("vendor"));

  unit->paths.generated = sp_fs_join_path(mem, unit->paths.work, SP_LIT("spn"));
  unit->paths.object = sp_fs_join_path(mem, unit->paths.generated, SP_LIT("object"));

  unit->logs.build = sp_fmt(mem, "{}.build.log", SP_FMT_STR(loaded->info->name)).value;
  unit->logs.jsonl = sp_fmt(mem, "{}.jsonl", SP_FMT_STR(loaded->info->name)).value;
  unit->paths.logs.build = sp_fs_join_path(mem, unit->paths.work, unit->logs.build);
  unit->paths.logs.jsonl = sp_fs_join_path(mem, unit->paths.work, unit->logs.jsonl);
  spn_lazy_log_init(&unit->logs.io.build, unit->paths.logs.build);
  spn_lazy_log_init(&unit->logs.io.jsonl, unit->paths.logs.jsonl);

  unit->paths.stamp.dir = sp_fs_join_path(mem, unit->paths.generated, SP_LIT("stamp"));
  unit->paths.stamp.configure = sp_fs_join_path(mem, unit->paths.stamp.dir, SP_LIT("configure.stamp"));
  unit->paths.stamp.package = sp_fs_join_path(mem, unit->paths.stamp.dir, SP_LIT("package.stamp"));
}
