#include "unit/unit.h"

#include "ctx/types.h"

static sp_str_t target_kind_dir(spn_target_kind_t kind) {
  switch (kind) {
    case SPN_TARGET_KIND_LIB:                   return sp_str_lit("lib");
    case SPN_TARGET_KIND_EXE:                   return sp_str_lit("exe");
    case SPN_TARGET_KIND_SCRIPT:                return sp_str_lit("script");
    case SPN_TARGET_KIND_TEST:                  return sp_str_lit("test");
    case SPN_TARGET_KIND_EXAMPLE:               return sp_str_lit("example");
    case SPN_TARGET_KIND_CONFIGURE_METAPROGRAM: return sp_str_lit("configure");
    case SPN_TARGET_KIND_BUILD_METAPROGRAM:     return sp_str_lit("build");
  }
  sp_unreachable_return(sp_str_lit(""));
}

sp_str_t spn_target_unit_object_dir(sp_mem_t mem, spn_target_unit_t* target) {
  if (target->lib_kind == SPN_LIB_KIND_OBJECT) {
    return target->pkg->paths.lib;
  }
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t kind = sp_fs_join_path(s.mem, target->pkg->paths.object, target_kind_dir(target->info->kind));
  sp_str_t dir = sp_fs_join_path(mem, kind, target->info->name);
  sp_mem_end_scratch(s);
  return dir;
}

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
